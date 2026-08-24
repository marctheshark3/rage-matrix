//! Host twin for the Maker LEDDisplay2 32×9.
//! Firmware on the ESP8266 stays C++ (Xtensa + HTTP OTA). This crate is the
//! place to measure and optimize the *algorithm* without flashing.

pub mod rng;
pub mod tank;
pub mod war;

pub use rng::Rng;
pub use tank::Tank;
pub use war::War;

pub const VW: usize = 32;
pub const VH: usize = 9;

#[inline]
pub fn clamp(x: f32, a: f32, b: f32) -> f32 {
    if x < a {
        a
    } else if x > b {
        b
    } else {
        x
    }
}

#[inline]
pub fn wrap(v: f32, m: f32) -> f32 {
    let mut x = v % m;
    if x < 0.0 {
        x += m;
    }
    x
}

#[inline]
pub fn wrapd(d: f32, m: f32) -> f32 {
    let half = m * 0.5;
    if d > half {
        d - m
    } else if d < -half {
        d + m
    } else {
        d
    }
}

#[inline]
pub fn steer(a: &mut f32, dx: f32, dy: f32, turn: f32, k: f32) {
    let want = dy.atan2(dx);
    let mut da = want - *a;
    const PI: f32 = core::f32::consts::PI;
    const TAU: f32 = core::f32::consts::TAU;
    da = (da + PI).rem_euclid(TAU) - PI;
    *a += da.clamp(-turn, turn) * k;
}

pub fn fb_hex(fb: &[[u8; VW]; VH]) -> String {
    let mut s = String::with_capacity(VW * VH * 2);
    for y in 0..VH {
        for x in 0..VW {
            s.push_str(&format!("{:02x}", fb[y][x]));
        }
    }
    s
}
