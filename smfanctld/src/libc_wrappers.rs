#![allow(unused)]

use std::ffi::CString;
use std::io::Error;

fn check_err<T: Ord + Default>(err: T) -> Result<T, Error> {
    if err < T::default() {
        return Err(Error::last_os_error());
    }
    Ok(err)
}

pub fn mkfifo(pathname: &str, mode: libc::mode_t) -> Result<u32, Error> {
    let pathname: CString = CString::new(pathname.as_bytes())?;
    check_err(unsafe { libc::mkfifo(pathname.as_ptr(), mode) }).map(|pid| pid as u32)
}
