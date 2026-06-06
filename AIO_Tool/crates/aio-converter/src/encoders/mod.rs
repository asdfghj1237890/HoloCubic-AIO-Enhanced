//! LVGL color-format encoders.
//!
//! Algorithm port of `AIO_Tool/util/convertor_core.py` which is:
//! > Copyright (c) 2021 W-Mai
//! > Licensed under the MIT License.
//! > <https://github.com/W-Mai/lvgl_image_converter>
//!
//! Rust port: see `aio-converter/README.md`.

pub mod alpha;
pub mod c_array;
pub mod indexed;
pub mod rgb;
