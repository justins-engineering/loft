use crate::components::{SetSessionCookie, session_cookie_valid};
use crate::config::{KRATOS_BROWSER_URL, SESSION_COOKIE_NAME};
use dioxus::prelude::*;
use dioxus_i18n::prelude::*;
use ory_kratos_client_wasm::apis::configuration::Configuration;
use std::collections::HashMap;
use thingspace_sdk::models::{CallbackListener, Device};
use unic_langid::langid;
use views::{
  CallbackView, DeviceView, Index, LoginFlow, PageNotFound, ServerError, SessionInfo, Settings,
  SettingsFlow, SignIn, Unauthorized, Wrapper,
};

pub mod api;
mod components;
mod config;
// mod encoding;
mod views;
// mod websocket;

#[derive(Clone, Copy, Debug)]
struct Session {
  state: Signal<bool>,
}

trait Create {
  fn create() -> Configuration;
}

impl Create for Configuration {
  fn create() -> Configuration {
    Configuration {
      base_path: KRATOS_BROWSER_URL.to_owned(),
      user_agent: None,
      basic_auth: None,
      oauth_access_token: None,
      bearer_access_token: None,
      api_key: None,
    }
  }
}

#[derive(Routable, Clone, PartialEq)]
#[rustfmt::skip]
enum Route {
#[layout(Wrapper)]
  #[layout(AuthGuard)]
    #[route("/")]
    Index {},
    #[route("/callback_listeners")]
    CallbackView {},
    #[route("/device/:id")]
    DeviceView { id: String },
    #[route("/session")]
    SessionInfo {},
    #[route("/my-settings")]
    Settings {},
    #[route("/settings?:flow")]
    SettingsFlow { flow: String },
  #[end_layout]
  #[route("/session/local?:state")]
  SetSessionCookie { state: bool },
  #[route("/sign-in")]
  SignIn {},
  #[route("/login?:flow")]
  LoginFlow { flow: String },
  #[route("/error?:id")]
  ServerError { id: String },
  #[route("/unauthorized")]
  Unauthorized {},
  #[route("/:..route")]
  PageNotFound { route: Vec<String> },
}

#[component]
fn AuthGuard() -> Element {
  if !*use_context::<Session>().state.read() {
    let nav = use_navigator();
    nav.replace(Route::Unauthorized {});
    return rsx! {};
  }

  rsx! {
    Outlet::<Route> {}
  }
}

#[derive(Clone, Copy, Debug)]
struct LocalSession {
  listeners: Signal<Vec<CallbackListener>>,
  devices: Signal<HashMap<String, Device>>,
}

#[component]
pub fn App() -> Element {
  use_init_i18n(|| {
    I18nConfig::new(langid!("en-US")).with_locale(Locale::new_static(
      langid!("en-US"),
      include_str!("../locales/en-US.ftl"),
    ))
  });

  use_effect(crate::components::set_lang);

  use_context_provider(|| Session {
    state: Signal::new(false),
  });

  let set_state = use_resource(move || async move { session_cookie_valid().await });
  (set_state)();

  use_context_provider(|| LocalSession {
    listeners: Signal::new(vec![CallbackListener::default()]),
    devices: Signal::new(HashMap::<String, Device>::new()),
  });

  rsx! {
    document::Link { rel: "stylesheet", href: asset!("/assets/styling/main.css") }
    document::Link {
      rel: "icon",
      href: asset!("/assets/images/icon-light.ico"),
      sizes: "32x32",
      media: "(prefers-color-scheme: light)",
    }
    document::Link {
      rel: "icon",
      href: asset!("/assets/images/icon-dark.ico"),
      sizes: "32x32",
      media: "(prefers-color-scheme: dark)",
    }
    document::Link {
      rel: "icon",
      r#type: "image/svg+xml",
      href: asset!("/assets/images/icon-light.svg"),
    }
    document::Link {
      rel: "icon",
      r#type: "image/svg+xml",
      href: asset!("/assets/images/icon-light.svg"),
      media: "(prefers-color-scheme: light)",
    }
    document::Link {
      rel: "icon",
      r#type: "image/svg+xml",
      href: asset!("/assets/images/icon-dark.svg"),
      media: "(prefers-color-scheme: dark)",
    }
    Router::<Route> {}
  }
}
