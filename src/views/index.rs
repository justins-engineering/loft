use crate::views::DevicesView;
use dioxus::prelude::*;

#[component]
pub fn Index() -> Element {
  rsx! {
    DevicesView {}
  }
}
