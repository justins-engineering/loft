# Loft
A place for our pigeons to rest. The main dashboard for PigIoT. A web app created in Dioxus, designed to be used with Cloudflare workers.

## Development

### Requirements
- [Bun](https://bun.com/get)
- [Dioxus CLI](https://dioxuslabs.com/learn/0.7/getting_started/)

### Tailwind CSS
1. Run the following command in the root of the project:
```bash
bun install
```
2. Run the following command in the root of the project to start the Tailwind CSS compiler:

```bash
bunx @tailwindcss/cli -i ./assets/tailwind.css -o ./assets/styling/main.css --watch
```

### Serving The App

Run the following command in the root of your project to start developing with the default platform:

```bash
dx serve --addr 127.0.0.1 --port 4455
```
