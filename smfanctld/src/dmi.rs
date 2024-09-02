use std::{fs::File, io::Read};

use log::debug;

#[derive(thiserror::Error, Debug)]
pub enum Error {
    #[error("this board is not the target board")]
    IsNotTargetBoard,
}

pub fn board_name() -> Result<String, std::io::Error> {
    let mut buf = String::new();
    let mut file = File::open("/sys/class/dmi/id/board_name")?;
    file.read_to_string(&mut buf)?;
    Ok(buf.trim().to_string())
}

pub fn board_vendor() -> Result<String, std::io::Error> {
    let mut buf = String::new();
    let mut file = File::open("/sys/class/dmi/id/board_vendor")?;
    file.read_to_string(&mut buf)?;
    Ok(buf.trim().to_string())
}

pub fn board_version() -> Result<String, std::io::Error> {
    let mut buf = String::new();
    let mut file = File::open("/sys/class/dmi/id/board_version")?;
    file.read_to_string(&mut buf)?;
    Ok(buf.trim().to_string())
}

pub fn board_is_target(
    target_name: &str,
    target_vendor: &str,
    target_version: &str,
) -> Result<(), Box<dyn std::error::Error>> {
    let name = board_name()?;
    let vendor = board_vendor()?;
    let version = board_version()?;

    debug!("Board name is {}", name);
    debug!("Board vendor is {}", vendor);
    debug!("Board version is {}", version);

    if name == target_name && vendor == target_vendor && version == target_version {
        Ok(())
    } else {
        Err(Box::new(Error::IsNotTargetBoard))
    }
}
