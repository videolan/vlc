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

use std::{cell::UnsafeCell, sync::Mutex};
use telegraf::{Client, IntoFieldData};
use vlcrs_core::tracer::{TraceValue, TracerCapability, TracerModuleLoader};
use vlcrs_macros::module;

struct TraceValueWrapper<'a>(TraceValue<'a>);

impl<'a> IntoFieldData for TraceValueWrapper<'a> {
    fn field_data(&self) -> telegraf::FieldData {
        match self.0 {
            TraceValue::String(value) => telegraf::FieldData::Str(String::from(value)),
            TraceValue::Integer(value) => telegraf::FieldData::Number(value),
            TraceValue::Unsigned(value) => telegraf::FieldData::UNumber(value),
            TraceValue::Double(value) => telegraf::FieldData::Float(value),
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

    fn trace(&self, _tick: vlcrs_core::tracer::Tick, trace: &vlcrs_core::tracer::Trace) {
        let (tags, records) = trace.entries().fold(
            (Vec::new(), Vec::new()),
            |(mut tags, mut records), entry| {
                let name = String::from(entry.key);
                if let TraceValue::String(value) = entry.value {
                    let value = String::from(value);
                    tags.push(telegraf::protocol::Tag { name, value });
                } else {
                    let value = TraceValueWrapper(entry.value).field_data();
                    records.push(telegraf::protocol::Field { name, value });
                }

                (tags, records)
            },
        );

        if records.is_empty() {
            /* We cannot support events for now. */
            return;
        }

        let p = telegraf::Point {
            measurement: String::from("measurement"),
            tags,
            fields: records,
            timestamp: None, //Some(tick.0 as u64),
        };

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
