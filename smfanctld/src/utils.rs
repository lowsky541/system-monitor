pub fn percent(current: f64, min: f64, max: f64) -> f64 {
    ((current - min) * 100.0f64) / (max - min)
}
