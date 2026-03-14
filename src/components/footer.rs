use chrono::Datelike;
use dioxus::prelude::*;

#[component]
pub fn Footer() -> Element {
  rsx! {
    footer { class: "footer footer-horizontal bg-neutral border-t border-neutral-content/10 pt-2",
      aside { class: "px-4 sm:px-6 w-full flex flex-col md:flex-row justify-between items-center",
        p { class: "mb-1 md:mb-0 text-sm",
          "© {chrono::Utc::now().year()} Justin's Engineering Services, LLC."
        }
        div { class: "flex items-center space-x-6",
          a {
            class: "hover:text-primary transition-colors duration-300 text-sm",
            href: "#",
            "Privacy Policy"
          }
          a {
            class: "hover:text-primary transition-colors duration-300 text-sm",
            href: "#",
            "Terms of Service"
          }
          a {
            class: "hover:text-primary transition-colors duration-300 text-sm",
            href: "#",
            "Cookie Policy"
          }
        }
      }
    }
  }
}
