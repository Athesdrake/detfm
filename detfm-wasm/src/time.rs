use std::{
    ops::{Add, Sub},
    time::Duration,
};

#[derive(Debug, Clone, Copy)]
pub struct JsInstant {
    timestamp: u64,
}

impl JsInstant {
    pub fn now() -> Self {
        Self {
            timestamp: Self::now_millis(),
        }
    }

    pub fn elapsed(self) -> Duration {
        Duration::from_millis(Self::now_millis() - self.timestamp)
    }

    #[allow(clippy::cast_possible_truncation, clippy::cast_sign_loss)]
    fn now_millis() -> u64 {
        web_sys::window()
            .expect("Window is not available")
            .performance()
            .expect("window.performance is not available")
            .now() as u64
    }
}

impl Add<Duration> for JsInstant {
    type Output = JsInstant;

    #[allow(clippy::cast_possible_truncation)]
    fn add(self, rhs: Duration) -> Self::Output {
        Self {
            timestamp: self.timestamp + rhs.as_millis() as u64,
        }
    }
}
impl Sub for JsInstant {
    type Output = Duration;

    fn sub(self, rhs: Self) -> Self::Output {
        Duration::from_millis(self.timestamp - rhs.timestamp)
    }
}
