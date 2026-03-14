# ── Stage 1: Build ────────────────────────────────────────────────────────────
FROM node:20-alpine AS build

WORKDIR /app

COPY package*.json ./
RUN npm ci

COPY . .

# ESP32 URL is baked into the Vite bundle at build time
ARG VITE_ESP32_URL=http://trm.local
ENV VITE_ESP32_URL=$VITE_ESP32_URL

# Production build (VITE_USE_MOCK is not set, so real API is used)
RUN npm run build

# ── Stage 2: Serve ────────────────────────────────────────────────────────────
FROM nginxinc/nginx-unprivileged:alpine

COPY --from=build /app/build /usr/share/nginx/html
COPY nginx.conf /etc/nginx/conf.d/default.conf

EXPOSE 8080
