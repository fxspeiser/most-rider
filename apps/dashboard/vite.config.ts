import { defineConfig } from "vite";
import react from "@vitejs/plugin-react";

// Dashboard is built and served as static files from api-bridge (same
// origin as the REST/WebSocket API, port 8282) — see ADR-0008. `base: "./"`
// keeps asset URLs relative so it works whether mounted at "/" or a
// sub-path. design/tokens.css is copied to src/tokens.css by the
// predev/prebuild npm scripts (not committed, not imported from outside
// this project's root — avoids needing vite's dev-server fs.allow).
export default defineConfig({
  plugins: [react()],
  base: "./",
  server: {
    proxy: {
      "/api": "http://localhost:8282",
      "/ws": { target: "ws://localhost:8282", ws: true },
    },
  },
  build: {
    outDir: "dist",
  },
});
