use dioxus::prelude::*;
use dioxus_free_icons::Icon;
use dioxus_free_icons::icons::ld_icons::{LdMoon, LdSun};
use wasm_theme::theme_toggle;

#[component]
pub fn ThemeController() -> Element {
  use_effect(theme_toggle);

  rsx! {
    div { class: "fixed inset-e-5 bottom-5 z-10 flex flex-col items-center",
      label {
        class: "swap swap-rotate",
        aria_label: "Dark/light theme toggle",
        input { name: "theme-toggle", r#type: "checkbox", value: "dark" }
        Icon { class: "swap-on", icon: LdSun, title: "Sun icon" }
        Icon { class: "swap-off", icon: LdMoon, title: "Moon icon" }
      }
    }
  }
}
