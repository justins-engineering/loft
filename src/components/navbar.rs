// use crate::components::Logo;
use crate::components::OryLogOut;
use crate::{Route, Session};
use dioxus::prelude::*;
use dioxus_free_icons::Icon;
use dioxus_free_icons::icons::ld_icons::{LdBird, LdLogIn, LdMenu, LdTag, LdUserCog, LdX};

#[component]
pub fn Navbar() -> Element {
  let mut show_menu: Signal<bool> = use_signal(|| true);

  rsx! {
    header { class: "w-full z-50 backdrop-blur-md bg-base-200/80 border-b border-stone-800/50 shadow-lg",
      nav { class: "navbar sm:px-4 md:px-8 lg:px-16 xl:px-32 2xl:px-64",
        div { class: "navbar-start space-x-3",
          div { class: "size-10 rounded-full flex items-center justify-center bg-secondary/60 animate-glow",
            // Logo {}
            Icon { icon: LdBird, class: "size-6", title: "Logo" }
          }
          span { class: "text-primary text-3xl font-bold", "PigIoT" }
        }
        div { class: "navbar-center hidden md:flex space-x-8",
          ul { class: "menu menu-lg menu-horizontal",
            li {
              Link {
                to: Route::Index {},
                class: "hover:text-primary transition-colors duration-300 relative group",
                "Home"
                span { class: "absolute bottom-0 left-0 w-0 h-0.5 bg-primary group-hover:w-full transition-all duration-300" }
              }
            }
            li {
              Link {
                to: Route::CallbackView {},
                class: "hover:text-primary transition-colors duration-300 relative group",
                "Callback Listeners"
                span { class: "absolute bottom-0 left-0 w-0 h-0.5 bg-primary group-hover:w-full transition-all duration-300" }
              }
            }
          }
        }
        div { class: "navbar-end",
          div {
            class: "swap swap-rotate",
            class: if show_menu() { "swap-active" },
            aria_label: "open menu",
            onclick: move |_| {
                show_menu.toggle();
            },
            Icon { icon: LdMenu, class: "swap-on" }
            Icon { icon: LdX, class: "swap-off" }
          }
        }
      }
      div {
        class: "mt-4 pb-4 w-full md:w-1/3",
        class: if show_menu() { "hidden" },
        div { class: "menu menu-vertical space-y-3 justify-items-center w-full",
          if *use_context::<Session>().state.read() {
            Link {
              to: Route::CallbackView {},
              class: "font-medium hover:text-primary transition-colors py-3 px-4 rounded-lg",
              Icon {
                icon: LdTag,
                class: "inline align-text-bottom mr-3",
              }
              "Callback Listeners"
            }
            Link {
              to: Route::Settings {},
              class: "font-medium hover:text-primary transition-colors py-3 px-4 rounded-lg",
              Icon {
                icon: LdUserCog,
                class: "inline align-text-bottom mr-3",
              }
              "Settings"
            }
            OryLogOut {}
          } else {
            Link {
              to: Route::SignIn {},
              class: "font-medium hover:text-primary transition-colors py-3 px-4 rounded-lg",
              Icon {
                icon: LdLogIn,
                class: "inline align-text-bottom mr-3",
              }
              "Sign In"
            }
          }
        }
      }
    }
  }
}
