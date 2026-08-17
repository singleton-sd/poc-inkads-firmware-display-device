#pragma once

#include <Arduino.h>

// Offline subset copied from tokens.design.singletonsd.com. Keep the public
// Singleton variable names stable so pages can be refreshed from the source.
const char DESIGN_TOKENS_CSS[] PROGMEM = R"css(
:root,[data-theme="light"]{
  --ssd-color-black:#000000;--ssd-color-white:#ffffff;
  --ssd-color-gray-100:#f5f6f7;--ssd-color-gray-200:#e1e3e6;
  --ssd-color-gray-300:#c9ccd1;--ssd-color-gray-500:#9da1a6;
  --ssd-color-gray-700:#60656b;--ssd-color-gray-800:#44484d;
  --ssd-color-gray-900:#292c30;
  --ssd-color-yellow-light-500:#c89200;
  --ssd-color-yellow-light-600:#b57f00;
  --ssd-color-yellow-dark-300:#ffe082;
  --ssd-color-yellow-dark-500:#ffb300;
  --ssd-color-red-100:#fff5f5;--ssd-color-red-400:#fc8181;
  --ssd-color-red-700:#c53030;
  --ssd-color-green-100:#f0fff4;--ssd-color-green-400:#68d391;
  --ssd-color-green-700:#2f855a;
  --ssd-font-family-heading:system-ui,-apple-system,"Segoe UI",sans-serif;
  --ssd-font-family-body:system-ui,-apple-system,"Segoe UI",sans-serif;
  --ssd-font-weight-regular:400;--ssd-font-weight-medium:500;
  --ssd-font-weight-semibold:600;--ssd-font-weight-bold:700;
  --ssd-font-size-100:12px;--ssd-font-size-200:14px;
  --ssd-font-size-300:16px;--ssd-font-size-400:26px;
  --ssd-space-100:4px;--ssd-space-200:8px;--ssd-space-300:16px;
  --ssd-space-400:24px;--ssd-space-500:32px;--ssd-space-600:48px;
  --ssd-radius-sm:4px;--ssd-radius-md:8px;--ssd-radius-lg:12px;
  --ssd-radius-xl:16px;--ssd-radius-full:9999px;
  --ssd-border-width-default:1px;--ssd-border-width-strong:2px;
  --ssd-motion-duration-fast:150ms;--ssd-motion-duration-default:300ms;
  --ssd-line-height-body:1.5;
  --ssd-color-text-default:var(--ssd-color-yellow-light-500);
  --ssd-color-text-muted:var(--ssd-color-yellow-dark-500);
  --ssd-color-text-subtle:var(--ssd-color-yellow-dark-300);
  --ssd-color-text-on-brand:var(--ssd-color-gray-900);
  --ssd-color-text-link:var(--ssd-color-yellow-light-600);
  --ssd-color-background-default:var(--ssd-color-white);
  --ssd-color-background-muted:var(--ssd-color-gray-100);
  --ssd-color-background-subtle:var(--ssd-color-gray-200);
  --ssd-color-background-brand:var(--ssd-color-yellow-light-500);
  --ssd-color-background-brand-hovered:var(--ssd-color-yellow-light-600);
  --ssd-color-border-default:var(--ssd-color-gray-300);
  --ssd-color-border-focus:var(--ssd-color-yellow-light-500);
  --ssd-color-feedback-danger-text:var(--ssd-color-red-700);
  --ssd-color-feedback-danger-background:var(--ssd-color-red-100);
  --ssd-color-feedback-danger-border:var(--ssd-color-red-400);
  --ssd-color-feedback-success-text:var(--ssd-color-green-700);
  --ssd-color-feedback-success-background:var(--ssd-color-green-100);
  --ssd-color-feedback-success-border:var(--ssd-color-green-400);
}
)css";
