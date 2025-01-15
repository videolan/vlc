// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2024-2025 Alexandre Janniaux <ajanni@videolabs.io>
//
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation; either version 2.1 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.

use std::{cell::UnsafeCell, ffi::CStr, sync::Mutex};
use telegraf::{Client, IntoFieldData, Point};
use vlcrs_core::tracer::{sys::vlc_tracer_value_type, TracerCapability, TracerModuleLoader};
use vlcrs_macros::module;

struct TraceField(vlcrs_core::tracer::TraceField);

impl IntoFieldData for TraceField {
    fn field_data(&self) -> telegraf::FieldData {
        match self.0.kind() {
            vlc_tracer_value_type::String => unsafe {
                let value = CStr::from_ptr(self.0.value().string);
                telegraf::FieldData::Str(value.to_str().unwrap().to_string())
            },
            vlc_tracer_value_type::Integer => unsafe {
                telegraf::FieldData::Number(self.0.value().integer)
            },
            vlc_tracer_value_type::Double => unsafe {
                telegraf::FieldData::Float(self.0.value().double)
            },
            _ => unreachable!(),
        }
    }
}

struct TelegrafTracer {
    endpoint: Mutex<UnsafeCell<telegraf::Client>>,
}

impl TracerCapability for TelegrafTracer {
    fn open(_obj: &mut vlcrs_core::object::Object) -> Option<impl TracerCapability>
    where
        Self: Sized,
    {
        let endpoint_address =
            std::env::var("VLC_TELEGRAF_ENDPOINT").unwrap_or(String::from("tcp://localhost:8094"));
        let endpoint = Client::new(&endpoint_address)
            .map(UnsafeCell::new)
            .map(Mutex::new)
            .unwrap();
        Some(Self { endpoint })
    }

    fn trace(&self, _tick: vlcrs_core::tracer::Tick, entries: &'_ vlcrs_core::tracer::Trace) {
        if !entries
            .into_iter()
            .any(|e| e.kind() != vlcrs_core::tracer::sys::vlc_tracer_value_type::String)
        {
            /* We cannot support events for now. */
            return;
        }
        let tags: Vec<(String, String)> = entries
            .into_iter()
            .filter(|e| e.kind() == vlcrs_core::tracer::sys::vlc_tracer_value_type::String)
            .map(|entry| unsafe {
                let value = CStr::from_ptr(entry.value().string);
                (
                    String::from(entry.key()),
                    String::from(value.to_str().unwrap()),
                )
            })
            .collect();

        let record: Vec<(String, Box<dyn IntoFieldData + 'static>)> = entries
            .into_iter()
            .filter(|e| e.kind() != vlcrs_core::tracer::sys::vlc_tracer_value_type::String)
            .map(|entry| {
                (
                    String::from(entry.key()),
                    Box::new(TraceField(entry)) as Box<dyn IntoFieldData>,
                )
            })
            .collect();

        let p = Point::new(
            String::from("measurement"),
            tags,
            record,
            None, //Some(tick.0 as u64),
        );

        let mut endpoint = self.endpoint.lock().unwrap();
        if let Err(err) = endpoint.get_mut().write_point(&p) {
            match err {
                telegraf::TelegrafError::IoError(e) => eprintln!("TelegrafTracer: IO Error: {}", e),
                telegraf::TelegrafError::ConnectionError(s) => {
                    eprintln!("telegraf tracer: connection error: {}", s)
                }
                telegraf::TelegrafError::BadProtocol(_) => todo!(),
            }
        }
    }
}

module! {
    type: TelegrafTracer (TracerModuleLoader),
    capability: "tracer" @ 0,
    category: ADVANCED_MISC,
    description: "Tracer module forwarding the traces to a Telegraf endpoint",
    shortname: "Telegraf tracer",
    shortcuts: ["telegraf"],
}
