extern crate lazy_static;
extern crate nix;
extern crate regex;
extern crate timer;

mod dmi;
mod utils;

use lazy_static::lazy_static;
use log::{debug, error, info};
use nix::unistd::geteuid;
use regex::Regex;
use std::fs::{File, OpenOptions};
use std::io::{Read, Write};
use std::os::unix::net::{UnixListener, UnixStream};
use std::os::unix::prelude::FileExt;
use std::path::Path;
use std::process::ExitCode;
use std::sync::atomic::{AtomicBool, AtomicU8};
use std::sync::{Arc, Mutex};
use std::vec;
use timer::Timer;

// Declare hardware constants
const PC_MB_NAME: &str = "868C";
const PC_MB_VENDOR: &str = "HP";
const PC_MB_VERSION: &str = "63.14";

// Declare fan parameters
const FAN_MIN_SPEED: u8 = 0;
const FAN_MAX_SPEED: u8 = 16;
const FAN_EC_READ_REGISTER: u8 = 113;
const FAN_EC_WRITE_REGISTER: u8 = 219;

// Declare fan ec parameters
const TASK_FAN_READ_INTERVAL_MS: i64 = 200;
const TASK_FAN_WRITE_INTERVAL_MS: i64 = 1000;

// Declare paths
const SOCKET_PATH: &str = concat!("/run/", env!("CARGO_BIN_NAME"), ".sock");
const EC0_IO_PATH: &str = "/sys/kernel/debug/ec/ec0/io";

// Declare regex that will be compiled only once
lazy_static! {
    static ref RE_GET_FAN_SPEED: Regex = Regex::new(r"^get$").unwrap();
    static ref RE_SET_FAN_SPEED: Regex = Regex::new(r"(?m)^set:(?P<speed>\d{1,3})$").unwrap();
    static ref RE_SET_FAN_AUTO: Regex = Regex::new(r"^auto$").unwrap();
}

static IS_AUTOMATICALLY_MANAGED: AtomicBool = AtomicBool::new(true);

fn read_fan_speed(ec_io: Arc<Mutex<File>>) -> std::io::Result<u8> {
    let mut buf = [0u8; 1];
    ec_io
        .lock()
        .unwrap()
        .read_exact_at(&mut buf, FAN_EC_READ_REGISTER as u64)?;
    Ok(buf[0])
}

fn write_fan_target_speed(ec_io: Arc<Mutex<File>>, speed: u8) -> std::io::Result<()> {
    let mut buf: [u8; 1] = [speed];
    ec_io
        .lock()
        .unwrap()
        .write_at(&mut buf, FAN_EC_WRITE_REGISTER as u64)?;
    Ok(())
}

fn handle_client(
    mut stream: UnixStream,
    current_speed: Arc<Mutex<u8>>,
    target_speed: Arc<Mutex<u8>>,
) {
    let mut buf = vec![0; 8];

    if let Err(e) = stream.read(&mut buf) {
        error!("Could not read packet: {}.", e);
        return;
    }

    let packet = match String::from_utf8(buf) {
        Ok(p) => p,
        Err(e) => {
            error!("Received in-decodable packet: {}.", e);
            return;
        }
    };

    // Remove all null-terminators (necessary for trim) then trim string
    let packet = packet.trim_matches(char::from(0)).trim();

    debug!("received packet: {:?}", packet);

    // Try to interpret the packet
    if let Some(capture) = RE_SET_FAN_SPEED.captures(packet) {
        // It should be safe to unwrap
        let speed = capture.name("speed").unwrap();
        let speed = match speed.as_str().parse::<u8>() {
            Ok(s) => s,
            Err(e) => {
                error!("Received unprocessable speed: {}.", e);
                return;
            }
        };

        if speed > 100 {
            error!("Received greater than 100 speed which is not a percentage.");
            return;
        }

        IS_AUTOMATICALLY_MANAGED.store(false, std::sync::atomic::Ordering::Relaxed);

        *target_speed.lock().unwrap() = speed;

        debug!("Requested to set target fan speed to {speed}.");
    } else if RE_SET_FAN_AUTO.is_match(packet) {
        IS_AUTOMATICALLY_MANAGED.store(true, std::sync::atomic::Ordering::Relaxed);

        debug!("Requested to set target fan speed to auto.");
    } else if RE_GET_FAN_SPEED.is_match(packet) {
        let speed = current_speed.lock().unwrap();

        let speed_data = speed.to_string();
        let speed_data = speed_data.as_bytes();

        match stream.write(speed_data) {
            Ok(_) => (),
            Err(e) => {
                error!("Could not write current fan speed to stream: {}.", e);
                return;
            }
        };

        debug!("Requested current fan speed which is {speed}.");
    }
}

fn main() -> ExitCode {
    simple_logger::init().unwrap();

    // Delete old socket file
    if let Ok(_) = std::fs::remove_file(SOCKET_PATH) {
        info!("Old socket file has been deleted.");
    }

    // Ensure the executable is running as root
    if !geteuid().is_root() {
        error!("This program must run with root privileges.");
        error!("This condition will not change, if you're unhappy with it, change me!");
        return ExitCode::FAILURE;
    }

    // Check the motherboard
    if let Err(e) = dmi::board_is_target(PC_MB_NAME, PC_MB_VENDOR, PC_MB_VERSION) {
        error!("This motherboard is not supported or the informations could not be retrieved.");
        error!("Error was: {}.", e);
        return ExitCode::FAILURE;
    }

    // Get the list of loaded modules
    let Ok(modules) = procfs::modules() else {
        error!("Could not get a list of loaded modules via procfs.");
        return ExitCode::FAILURE;
    };

    info!("Success getting loaded modules via procfs.");

    // Check if loaded modules contains 'ec_sys'
    if !modules.contains_key("ec_sys") {
        error!("Module ec_sys does not seem to be loaded.");
        error!("Try running `modprobe ec_sys write_support=1` with root privileges.");
        return ExitCode::FAILURE;
    }

    info!("Module ec_sys seem to be loaded.");

    // Check if write_support is enabled with any more depth
    if let Err(_) = Path::new("/sys/module/ec_sys/parameters/write_support").try_exists() {
        error!("'write_support' does not seem to be enabled for ec_sys module.");
        return ExitCode::FAILURE;
    }

    // Check if embedded controller io device file exists
    let ec_io = match OpenOptions::new().write(true).read(true).open(EC0_IO_PATH) {
        Ok(file) => Arc::new(Mutex::new(file)),
        Err(e) => {
            error!("Failure to open ec_sys ec0/io: {}.", e);
            return ExitCode::FAILURE;
        }
    };

    info!("ec_sys debug io has been opened.");

    // Keep track of the target speed and the current speed
    let current_speed = Arc::new(Mutex::new(0u8));
    let target_speed = Arc::new(Mutex::new(0u8));

    // A timer used to schedule function calls
    let scheduler = Timer::new();

    let read_failures = AtomicU8::new(0u8);

    // Execute a function every TASK_FAN_READ_INTERVAL_MS milliseconds
    // that reads the current fan speed from the EC.
    let _guard_task_read_fan_speed = {
        let current_speed = current_speed.clone();
        let ec_io = ec_io.clone();
        scheduler.schedule_repeating(
            chrono::Duration::milliseconds(TASK_FAN_READ_INTERVAL_MS),
            move || {
                let speed = match read_fan_speed(ec_io.clone()) {
                    Ok(v) => v,
                    Err(e) => {
                        error!("Cannot read current fan speed: {}.", e);
                        assert!(
                            read_failures.fetch_add(1, std::sync::atomic::Ordering::Relaxed) != 2
                        );
                        return;
                    }
                };

                let percent_speed =
                    utils::percent(speed as f64, FAN_MIN_SPEED as f64, FAN_MAX_SPEED as f64) as u8;

                *current_speed.lock().unwrap() = percent_speed;

                debug!(target: "smfanctl:reader", "Current fan speed is {speed}/{percent_speed}%.");
            },
        )
    };

    // Execute a function every TASK_FAN_WRITE_INTERVAL_MS milliseconds
    // that write a new target fan speed to the EC.
    let _guard_task_write_target_speed = {
        let target_speed = target_speed.clone();
        let ec_io = ec_io.clone();
        scheduler.schedule_repeating(
            chrono::Duration::milliseconds(TASK_FAN_WRITE_INTERVAL_MS),
            move || {
                let target_speed: u8 = *target_speed.lock().unwrap();

                // Only set the target fan speed if the fan is not self managed.
                if !IS_AUTOMATICALLY_MANAGED.load(std::sync::atomic::Ordering::Relaxed) {
                    debug!(target: "smfanctl:writer", "Setting fan target speed to {target_speed}");
                    match write_fan_target_speed(ec_io.clone(), target_speed) {
                        Ok(_) => (),
                        Err(e) => {
                            error!("Failure writing fan target speed: {}.", e);
                            return;
                        }
                    }
                }
            },
        )
    };

    // Create a new unix domain socket listening on SOCKET_PATH
    let listener = match UnixListener::bind(SOCKET_PATH) {
        Ok(l) => l,
        Err(e) => {
            error!("Could not bind to {}: {}.", SOCKET_PATH, e);
            return ExitCode::FAILURE;
        }
    };

    // Accept each connection in-serial
    for stream in listener.incoming() {
        let stream = match stream {
            Ok(result) => result,
            Err(e) => {
                error!("Failed to establish connection: {}", e);
                continue;
            }
        };

        handle_client(
            stream,
            Arc::clone(&current_speed),
            Arc::clone(&target_speed),
        );
    }

    return ExitCode::SUCCESS;
}
