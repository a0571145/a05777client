/*
 * Copyright (C) 2026 a05777
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#pragma once

// Single-file web UI for a05777Client. Served by RestServer on GET /.
// Page source lives at D:/zm1/web/index.html (user-designed dark pixel theme:
// category tabs, search/filter, collapsible cards, keybind record, debug
// inspector, toast + import summary modal, offline preview mode) and is
// embedded here verbatim. Vanilla HTML/CSS/JS, no CDN, no build step.
// ASCII only (MSVC charset safe): keep D:/zm1/web/index.html ASCII-only.
namespace raven {
namespace web {

inline constexpr const char* kIndexHtml = R"WEB(<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="US-ASCII">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>a05777Client</title>
  <style>
    /* Reset & Base Variables */
    *, *::before, *::after {
      box-sizing: border-box;
      margin: 0;
      padding: 0;
    }

    :root {
      --ink: #11140f;
      --ink-soft: #161a14;
      --ink-card: #191f16;
      --ink-card-hover: #1f271c;
      --ink-elevated: #242d20;
      --ink-input: #121610;
      --paper: #f4f0e6;
      --paper-dim: #c5c0b4;
      --muted: #85917c;
      --grass: #9dde4a;
      --grass-dark: #6da626;
      --grass-glow: rgba(157, 222, 74, 0.25);
      --lava: #ff7a2f;
      --lava-dark: #b8420d;
      --sky: #58bee8;
      --sky-dark: #177ea8;
      --line: rgba(244, 240, 230, 0.12);
      --line-bright: rgba(244, 240, 230, 0.24);
      --shadow-pixel: 3px 3px 0 rgba(0, 0, 0, 0.65);
      --font-display: Consolas, "Cascadia Code", "Courier New", monospace;
      --font-sans: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, "Helvetica Neue", Arial, sans-serif;
    }

    html {
      scroll-behavior: smooth;
    }

    body {
      min-height: 100vh;
      background-color: var(--ink);
      background-image: 
        linear-gradient(rgba(157, 222, 74, 0.035) 1px, transparent 1px),
        linear-gradient(90deg, rgba(157, 222, 74, 0.035) 1px, transparent 1px);
      background-size: 28px 28px;
      color: var(--paper);
      font-family: var(--font-sans);
      font-size: 14px;
      line-height: 1.5;
      -webkit-font-smoothing: antialiased;
    }

    /* Pixel Brand Cube Icon */
    .brand-cube {
      position: relative;
      width: 22px;
      height: 22px;
      border: 2px solid var(--ink);
      background: var(--grass);
      box-shadow: inset -4px -4px 0 var(--grass-dark), 2px 2px 0 var(--paper);
      flex-shrink: 0;
      image-rendering: pixelated;
    }

    .brand-cube::after {
      content: "";
      position: absolute;
      top: 2px;
      left: 2px;
      width: 5px;
      height: 5px;
      background: rgba(255, 255, 255, 0.7);
    }

    /* Pulsing Status Dot */
    .live-dot {
      display: inline-block;
      width: 8px;
      height: 8px;
      border-radius: 50%;
      background: var(--grass);
      box-shadow: 0 0 8px var(--grass);
      flex-shrink: 0;
    }

    .live-dot.pulse {
      animation: dotPulse 1.8s infinite ease-in-out;
    }

    .live-dot.offline {
      background: var(--lava);
      box-shadow: 0 0 8px var(--lava);
      animation: none;
    }

    .live-dot.connecting {
      background: var(--sky);
      box-shadow: 0 0 8px var(--sky);
    }

    @keyframes dotPulse {
      0%, 100% { transform: scale(1); opacity: 1; }
      50% { transform: scale(0.65); opacity: 0.45; }
    }

    /* Header Bar */
    .site-header {
      position: sticky;
      top: 0;
      z-index: 100;
      display: flex;
      align-items: center;
      justify-content: space-between;
      height: 64px;
      padding: 0 24px;
      background: rgba(17, 20, 15, 0.94);
      backdrop-filter: blur(10px);
      border-bottom: 1px solid var(--line);
      gap: 16px;
    }

    .brand-group {
      display: flex;
      align-items: center;
      gap: 12px;
      text-decoration: none;
      color: inherit;
    }

    .brand-title {
      font-family: var(--font-display);
      font-size: 16px;
      font-weight: 700;
      letter-spacing: -0.5px;
      color: var(--paper);
    }

    .brand-tag {
      font-family: var(--font-display);
      font-size: 11px;
      padding: 2px 7px;
      background: rgba(157, 222, 74, 0.12);
      border: 1px solid var(--grass-dark);
      color: var(--grass);
      border-radius: 2px;
      text-transform: uppercase;
    }

    /* Status Bar Info */
    .header-status {
      display: flex;
      align-items: center;
      gap: 14px;
      background: var(--ink-soft);
      border: 1px solid var(--line);
      padding: 5px 14px;
      border-radius: 4px;
      font-family: var(--font-display);
      font-size: 12px;
    }

    .status-item {
      display: flex;
      align-items: center;
      gap: 6px;
      color: var(--paper-dim);
    }

    .status-divider {
      width: 1px;
      height: 14px;
      background: var(--line);
    }

    .status-val {
      color: var(--paper);
      font-weight: 600;
    }

    .status-val.green {
      color: var(--grass);
    }

    .status-val.orange {
      color: var(--lava);
    }

    /* Header Actions */
    .header-actions {
      display: flex;
      align-items: center;
      gap: 8px;
    }

    /* Button Styles */
    .btn {
      display: inline-flex;
      align-items: center;
      justify-content: center;
      gap: 6px;
      padding: 7px 14px;
      font-family: var(--font-display);
      font-size: 12px;
      font-weight: 600;
      color: var(--paper);
      background: var(--ink-card);
      border: 1px solid var(--line-bright);
      border-radius: 3px;
      cursor: pointer;
      box-shadow: 2px 2px 0 rgba(0, 0, 0, 0.5);
      transition: all 0.15s ease;
      user-select: none;
      white-space: nowrap;
    }

    .btn:hover {
      background: var(--ink-card-hover);
      border-color: var(--grass);
      color: var(--grass);
      transform: translate(-1px, -1px);
      box-shadow: 3px 3px 0 rgba(0, 0, 0, 0.7);
    }

    .btn:active {
      transform: translate(1px, 1px);
      box-shadow: 1px 1px 0 rgba(0, 0, 0, 0.5);
    }

    .btn-primary {
      background: var(--grass);
      border-color: var(--grass-dark);
      color: var(--ink);
      box-shadow: 2px 2px 0 var(--grass-dark);
    }

    .btn-primary:hover {
      background: #a9e855;
      color: var(--ink);
      border-color: var(--grass);
      box-shadow: 3px 3px 0 var(--grass-dark);
    }

    .btn-lava {
      background: rgba(255, 122, 47, 0.15);
      border-color: var(--lava);
      color: var(--lava);
    }

    .btn-lava:hover {
      background: var(--lava);
      color: var(--ink);
      border-color: var(--lava-dark);
      box-shadow: 3px 3px 0 var(--lava-dark);
    }

    .btn-sm {
      padding: 4px 10px;
      font-size: 11px;
    }

    /* Offline Alert Bar */
    .offline-banner {
      display: none;
      background: rgba(255, 122, 47, 0.12);
      border-bottom: 1px solid rgba(255, 122, 47, 0.35);
      padding: 8px 24px;
      font-family: var(--font-display);
      font-size: 12px;
      color: var(--lava);
      align-items: center;
      justify-content: space-between;
      gap: 16px;
    }

    .offline-banner.active {
      display: flex;
    }

    .offline-banner a {
      color: var(--paper);
      text-decoration: underline;
      cursor: pointer;
    }

    /* Main Container */
    .main-wrap {
      max-width: 1440px;
      margin: 0 auto;
      padding: 24px;
    }

    /* Navigation & Categories */
    .nav-bar {
      display: flex;
      flex-direction: column;
      gap: 16px;
      margin-bottom: 24px;
    }

    .category-tabs {
      display: flex;
      align-items: center;
      gap: 8px;
      overflow-x: auto;
      padding-bottom: 6px;
      scrollbar-width: thin;
      scrollbar-color: var(--line-bright) transparent;
    }

    .tab-btn {
      display: inline-flex;
      align-items: center;
      gap: 6px;
      padding: 8px 14px;
      font-family: var(--font-display);
      font-size: 12px;
      font-weight: 600;
      color: var(--muted);
      background: var(--ink-card);
      border: 1px solid var(--line);
      border-radius: 3px;
      cursor: pointer;
      white-space: nowrap;
      transition: all 0.15s ease;
    }

    .tab-btn:hover {
      color: var(--paper);
      border-color: var(--line-bright);
      background: var(--ink-card-hover);
    }

    .tab-btn.active {
      color: var(--ink);
      background: var(--grass);
      border-color: var(--grass);
      box-shadow: 2px 2px 0 var(--grass-dark);
    }

    .tab-btn .badge {
      display: inline-block;
      font-size: 10px;
      padding: 1px 5px;
      border-radius: 2px;
      background: rgba(0, 0, 0, 0.2);
    }

    .tab-btn.active .badge {
      background: rgba(17, 20, 15, 0.25);
      color: var(--ink);
    }

    /* Search & Filter Row */
    .controls-row {
      display: flex;
      align-items: center;
      justify-content: space-between;
      gap: 16px;
      flex-wrap: wrap;
      background: var(--ink-soft);
      border: 1px solid var(--line);
      padding: 12px 16px;
      border-radius: 4px;
    }

    .search-box {
      position: relative;
      flex: 1;
      min-width: 240px;
      max-width: 440px;
    }

    .search-input {
      width: 100%;
      padding: 7px 12px 7px 32px;
      font-family: var(--font-display);
      font-size: 12px;
      color: var(--paper);
      background: var(--ink-input);
      border: 1px solid var(--line-bright);
      border-radius: 3px;
      outline: none;
      transition: border-color 0.15s ease;
    }

    .search-input:focus {
      border-color: var(--grass);
      box-shadow: 0 0 0 2px var(--grass-glow);
    }

    .search-icon {
      position: absolute;
      left: 10px;
      top: 50%;
      transform: translateY(-50%);
      color: var(--muted);
      pointer-events: none;
      font-size: 12px;
    }

    .filter-group {
      display: flex;
      align-items: center;
      gap: 10px;
      flex-wrap: wrap;
    }

    .filter-select {
      padding: 6px 12px;
      font-family: var(--font-display);
      font-size: 12px;
      color: var(--paper);
      background: var(--ink-card);
      border: 1px solid var(--line-bright);
      border-radius: 3px;
      outline: none;
      cursor: pointer;
    }

    .filter-select:focus {
      border-color: var(--grass);
    }

    .count-indicator {
      font-family: var(--font-display);
      font-size: 12px;
      color: var(--muted);
    }

    /* Module Grid */
    .module-grid {
      display: grid;
      grid-template-columns: repeat(auto-fill, minmax(360px, 1fr));
      gap: 16px;
    }

    /* Module Card */
    .module-card {
      background: var(--ink-card);
      border: 1px solid var(--line);
      border-radius: 4px;
      box-shadow: var(--shadow-pixel);
      transition: border-color 0.15s ease, background 0.15s ease;
      overflow: hidden;
      display: flex;
      flex-direction: column;
    }

    .module-card:hover {
      border-color: var(--line-bright);
      background: var(--ink-card-hover);
    }

    .module-card.is-enabled {
      border-left: 4px solid var(--grass);
    }

    /* Card Header */
    .card-header {
      display: flex;
      align-items: center;
      justify-content: space-between;
      padding: 12px 14px;
      gap: 10px;
      border-bottom: 1px solid transparent;
      user-select: none;
    }

    .module-card.has-settings .card-header {
      cursor: pointer;
    }

    .module-card.is-expanded .card-header {
      border-bottom-color: var(--line);
    }

    .header-left {
      display: flex;
      align-items: center;
      gap: 8px;
      min-width: 0;
    }

    .module-name {
      font-family: var(--font-display);
      font-size: 14px;
      font-weight: 700;
      color: var(--paper);
      overflow: hidden;
      text-overflow: ellipsis;
      white-space: nowrap;
    }

    .category-badge {
      font-family: var(--font-display);
      font-size: 10px;
      padding: 1px 6px;
      background: var(--ink-soft);
      border: 1px solid var(--line);
      color: var(--muted);
      border-radius: 2px;
    }

    .header-right {
      display: flex;
      align-items: center;
      gap: 8px;
      flex-shrink: 0;
    }

    /* Keybind Pill Button */
    .keybind-btn {
      font-family: var(--font-display);
      font-size: 11px;
      font-weight: 600;
      padding: 3px 8px;
      border-radius: 2px;
      background: var(--ink-elevated);
      border: 1px solid var(--line);
      color: var(--paper-dim);
      cursor: pointer;
      transition: all 0.15s ease;
    }

    .keybind-btn:hover {
      border-color: var(--sky);
      color: var(--sky);
    }

    .keybind-btn.recording {
      background: rgba(88, 190, 232, 0.2);
      border-color: var(--sky);
      color: var(--sky);
      animation: keyPulse 1s infinite alternate;
    }

    @keyframes keyPulse {
      0% { opacity: 0.7; }
      100% { opacity: 1; }
    }

    /* Toggle Switch */
    .toggle-switch {
      position: relative;
      display: inline-block;
      width: 36px;
      height: 20px;
      flex-shrink: 0;
      cursor: pointer;
    }

    .toggle-switch input {
      opacity: 0;
      width: 0;
      height: 0;
    }

    .toggle-track {
      position: absolute;
      inset: 0;
      background: #252e21;
      border: 1px solid var(--line-bright);
      border-radius: 10px;
      transition: all 0.2s ease;
    }

    .toggle-track::before {
      position: absolute;
      content: "";
      height: 12px;
      width: 12px;
      left: 3px;
      bottom: 3px;
      background: var(--muted);
      border-radius: 50%;
      transition: all 0.2s cubic-bezier(0.2, 0.8, 0.2, 1);
    }

    .toggle-switch input:checked + .toggle-track {
      background: rgba(157, 222, 74, 0.25);
      border-color: var(--grass);
    }

    .toggle-switch input:checked + .toggle-track::before {
      transform: translateX(16px);
      background: var(--grass);
      box-shadow: 0 0 6px rgba(157, 222, 74, 0.8);
    }

    .expand-icon {
      font-family: var(--font-display);
      font-size: 11px;
      color: var(--muted);
      transition: transform 0.2s ease;
      width: 14px;
      text-align: center;
    }

    .module-card.is-expanded .expand-icon {
      transform: rotate(90deg);
      color: var(--grass);
    }

    /* Card Settings Section */
    .card-body {
      display: none;
      padding: 12px 14px;
      flex-direction: column;
      gap: 12px;
      background: rgba(0, 0, 0, 0.15);
    }

    .module-card.is-expanded .card-body {
      display: flex;
    }

    /* Setting Items */
    .setting-group {
      display: flex;
      flex-direction: column;
      gap: 6px;
    }

    .setting-group-title {
      font-family: var(--font-display);
      font-size: 10px;
      color: var(--muted);
      text-transform: uppercase;
      letter-spacing: 0.5px;
    }

    /* Boolean Setting */
    .setting-bool {
      display: flex;
      align-items: center;
      justify-content: space-between;
      padding: 5px 8px;
      background: var(--ink-card);
      border: 1px solid var(--line);
      border-radius: 3px;
    }

    .setting-bool-label {
      font-size: 12px;
      color: var(--paper-dim);
    }

    /* Slider Setting */
    .setting-slider {
      display: flex;
      flex-direction: column;
      gap: 5px;
      padding: 6px 8px;
      background: var(--ink-card);
      border: 1px solid var(--line);
      border-radius: 3px;
    }

    .slider-header {
      display: flex;
      align-items: center;
      justify-content: space-between;
      font-size: 12px;
    }

    .slider-name {
      color: var(--paper-dim);
    }

    .slider-badge {
      font-family: var(--font-display);
      font-size: 11px;
      color: var(--grass);
      background: var(--ink-soft);
      padding: 1px 6px;
      border: 1px solid var(--line);
      border-radius: 2px;
      min-width: 44px;
      text-align: right;
      cursor: text;
    }

    .slider-range-wrap {
      display: flex;
      align-items: center;
      gap: 8px;
    }

    .slider-range-wrap span {
      font-family: var(--font-display);
      font-size: 10px;
      color: var(--muted);
      width: 28px;
    }

    .slider-range-wrap span.max {
      text-align: right;
    }

    input[type="range"] {
      -webkit-appearance: none;
      appearance: none;
      flex: 1;
      height: 4px;
      background: #252e21;
      border-radius: 2px;
      outline: none;
      cursor: pointer;
    }

    input[type="range"]::-webkit-slider-thumb {
      -webkit-appearance: none;
      appearance: none;
      width: 14px;
      height: 14px;
      background: var(--grass);
      border: 2px solid var(--ink);
      border-radius: 2px;
      box-shadow: 1px 1px 0 rgba(0, 0, 0, 0.5);
      cursor: pointer;
      transition: transform 0.1s ease;
    }

    input[type="range"]::-webkit-slider-thumb:hover {
      transform: scale(1.15);
    }

    /* Mode Setting */
    .setting-mode {
      display: flex;
      flex-direction: column;
      gap: 6px;
      padding: 6px 8px;
      background: var(--ink-card);
      border: 1px solid var(--line);
      border-radius: 3px;
    }

    .mode-header {
      font-size: 12px;
      color: var(--paper-dim);
    }

    .mode-options-row {
      display: flex;
      gap: 4px;
      flex-wrap: wrap;
    }

    .mode-btn {
      padding: 3px 8px;
      font-family: var(--font-display);
      font-size: 11px;
      background: var(--ink-elevated);
      border: 1px solid var(--line);
      color: var(--paper-dim);
      border-radius: 2px;
      cursor: pointer;
      transition: all 0.15s ease;
    }

    .mode-btn:hover {
      border-color: var(--grass);
      color: var(--paper);
    }

    .mode-btn.active {
      background: var(--grass);
      border-color: var(--grass-dark);
      color: var(--ink);
      font-weight: 700;
    }

    /* Color Setting */
    .setting-color {
      display: flex;
      align-items: center;
      justify-content: space-between;
      padding: 6px 8px;
      background: var(--ink-card);
      border: 1px solid var(--line);
      border-radius: 3px;
    }

    .color-label {
      font-size: 12px;
      color: var(--paper-dim);
    }

    .color-picker-wrap {
      display: flex;
      align-items: center;
      gap: 6px;
      position: relative;
    }

    .color-preview {
      width: 20px;
      height: 20px;
      border: 2px solid var(--line-bright);
      border-radius: 3px;
      cursor: pointer;
      box-shadow: 1px 1px 0 rgba(0, 0, 0, 0.4);
    }

    .color-hex {
      font-family: var(--font-display);
      font-size: 11px;
      color: var(--paper-dim);
      background: var(--ink-soft);
      border: 1px solid var(--line);
      padding: 2px 6px;
      border-radius: 2px;
      width: 64px;
      text-align: center;
    }

    input[type="color"] {
      position: absolute;
      opacity: 0;
      pointer-events: none;
      width: 0;
      height: 0;
    }

    /* Debug Inspector Section */
    .debug-section {
      display: none;
      flex-direction: column;
      gap: 20px;
      margin-top: 16px;
    }

    .debug-section.active {
      display: flex;
    }

    .debug-card {
      background: var(--ink-card);
      border: 1px solid var(--line);
      border-radius: 4px;
      padding: 16px;
      box-shadow: var(--shadow-pixel);
    }

    .debug-card-title {
      font-family: var(--font-display);
      font-size: 13px;
      font-weight: 700;
      color: var(--grass);
      margin-bottom: 12px;
      display: flex;
      align-items: center;
      justify-content: space-between;
    }

    .debug-table {
      width: 100%;
      border-collapse: collapse;
      font-family: var(--font-display);
      font-size: 12px;
    }

    .debug-table th, .debug-table td {
      padding: 6px 10px;
      text-align: left;
      border-bottom: 1px solid var(--line);
    }

    .debug-table th {
      color: var(--muted);
      font-weight: 600;
      width: 30%;
    }

    .debug-table td {
      color: var(--paper);
    }

    .debug-json-box {
      font-family: var(--font-display);
      font-size: 11px;
      background: var(--ink-input);
      border: 1px solid var(--line);
      padding: 12px;
      border-radius: 3px;
      max-height: 240px;
      overflow-y: auto;
      white-space: pre-wrap;
      word-break: break-all;
      color: #9cdcfe;
    }

    /* Modal Backdrop */
    .modal-backdrop {
      position: fixed;
      inset: 0;
      background: rgba(0, 0, 0, 0.7);
      backdrop-filter: blur(4px);
      z-index: 1000;
      display: none;
      align-items: center;
      justify-content: center;
      padding: 20px;
    }

    .modal-backdrop.open {
      display: flex;
    }

    .modal-dialog {
      background: var(--ink-card);
      border: 2px solid var(--line-bright);
      border-radius: 4px;
      box-shadow: 6px 6px 0 rgba(0, 0, 0, 0.8);
      width: 100%;
      max-width: 500px;
      overflow: hidden;
    }

    .modal-header {
      display: flex;
      align-items: center;
      justify-content: space-between;
      padding: 14px 18px;
      border-bottom: 1px solid var(--line);
      font-family: var(--font-display);
      font-size: 14px;
      font-weight: 700;
      color: var(--paper);
    }

    .modal-body {
      padding: 18px;
      font-size: 13px;
      color: var(--paper-dim);
      display: flex;
      flex-direction: column;
      gap: 12px;
    }

    .modal-footer {
      display: flex;
      align-items: center;
      justify-content: flex-end;
      gap: 10px;
      padding: 12px 18px;
      border-top: 1px solid var(--line);
      background: var(--ink-soft);
    }

    /* Toast Container */
    .toast-container {
      position: fixed;
      bottom: 24px;
      right: 24px;
      display: flex;
      flex-direction: column;
      gap: 8px;
      z-index: 2000;
      pointer-events: none;
    }

    .toast {
      pointer-events: auto;
      display: flex;
      align-items: center;
      gap: 10px;
      padding: 10px 16px;
      background: var(--ink-card);
      border: 1px solid var(--line-bright);
      border-radius: 3px;
      box-shadow: var(--shadow-pixel);
      font-family: var(--font-display);
      font-size: 12px;
      color: var(--paper);
      animation: toastIn 0.2s cubic-bezier(0.2, 0.8, 0.2, 1);
      max-width: 360px;
    }

    .toast.success {
      border-left: 4px solid var(--grass);
    }

    .toast.error {
      border-left: 4px solid var(--lava);
      color: var(--lava);
    }

    .toast.info {
      border-left: 4px solid var(--sky);
      color: var(--sky);
    }

    @keyframes toastIn {
      from { transform: translateY(16px); opacity: 0; }
      to { transform: translateY(0); opacity: 1; }
    }

    /* Scrollbar */
    ::-webkit-scrollbar {
      width: 6px;
      height: 6px;
    }
    ::-webkit-scrollbar-track {
      background: var(--ink);
    }
    ::-webkit-scrollbar-thumb {
      background: var(--line-bright);
      border-radius: 3px;
    }
    ::-webkit-scrollbar-thumb:hover {
      background: var(--muted);
    }

    /* Empty state */
    .empty-state {
      grid-column: 1 / -1;
      text-align: center;
      padding: 48px 16px;
      color: var(--muted);
      font-family: var(--font-display);
      font-size: 13px;
    }
  </style>
</head>
<body>

  <!-- Top Header -->
  <header class="site-header">
    <div class="brand-group">
      <div class="brand-cube" aria-hidden="true"></div>
      <span class="brand-title">a05777Client</span>
      <span class="brand-tag" id="game-tag">1.8.9</span>
    </div>

    <!-- Live Status Overview -->
    <div class="header-status">
      <div class="status-item">
        <span class="live-dot connecting" id="status-dot"></span>
        <span class="status-val" id="status-conn">CONNECTING</span>
      </div>
      <div class="status-divider"></div>
      <div class="status-item">
        <span>STATE:</span>
        <span class="status-val green" id="status-state">RUNNING</span>
      </div>
      <div class="status-divider"></div>
      <div class="status-item">
        <span>ACTIVE:</span>
        <span class="status-val" id="status-active">0 / 0</span>
      </div>
      <div class="status-divider"></div>
      <div class="status-item">
        <span>FPS:</span>
        <span class="status-val" id="status-fps">--</span>
      </div>
    </div>

    <!-- Header Quick Actions -->
    <div class="header-actions">
      <button class="btn btn-sm" id="btn-refresh" title="Refresh client status & modules">
        <svg width="12" height="12" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5"><path d="M21 2v6h-6M3 12a9 9 0 0 1 15-6.7L21 8M3 22v-6h6M21 12a9 9 0 0 1-15 6.7L3 16"/></svg>
        <span>Refresh</span>
      </button>

      <button class="btn btn-sm" id="btn-export" title="Export config to JSON file">
        <svg width="12" height="12" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5"><path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4M7 10l5 5 5-5M12 15V3"/></svg>
        <span>Export</span>
      </button>

      <label class="btn btn-sm" title="Import config from JSON file">
        <svg width="12" height="12" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5"><path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4M17 8l-5-5-5 5M12 3v12"/></svg>
        <span>Import</span>
        <input type="file" id="file-import" accept=".json" style="display:none;">
      </label>

      <button class="btn btn-sm btn-lava" id="btn-toggle-debug" title="Toggle debug inspection panel">
        <span>Debug</span>
      </button>
    </div>
  </header>

  <!-- Offline / Fallback Banner -->
  <div class="offline-banner" id="offline-banner">
    <span>[OFFLINE MODE] Cannot reach API at 127.0.0.1:8080. Running in local preview mode. Changes are simulated.</span>
    <a id="banner-retry">Retry Connection</a>
  </div>

  <!-- Main Body -->
  <main class="main-wrap">

    <!-- Categories & Navigation -->
    <div class="nav-bar">
      <div class="category-tabs" id="category-tabs">
        <!-- Rendered dynamically -->
      </div>

      <!-- Search & Filters -->
      <div class="controls-row">
        <div class="search-box">
          <span class="search-icon">/</span>
          <input type="text" class="search-input" id="search-input" placeholder="Search module name or setting...">
        </div>

        <div class="filter-group">
          <select class="filter-select" id="filter-state">
            <option value="all">All Modules</option>
            <option value="enabled">Enabled Only</option>
            <option value="disabled">Disabled Only</option>
          </select>

          <button class="btn btn-sm" id="btn-expand-all">Expand All</button>
          <button class="btn btn-sm" id="btn-collapse-all">Collapse All</button>

          <span class="count-indicator" id="count-indicator">Showing 0 modules</span>
        </div>
      </div>
    </div>

    <!-- Modules Grid -->
    <div class="module-grid" id="module-grid">
      <!-- Module Cards Rendered Here -->
    </div>

    <!-- Debug Inspector Panel -->
    <div class="debug-section" id="debug-section">
      <div class="controls-row" style="margin-bottom: 0;">
        <div style="font-family: var(--font-display); font-weight: 700; color: var(--lava);">
          SYSTEM DIAGNOSTICS & DEBUG
        </div>
        <div style="display: flex; gap: 8px; align-items: center;">
          <label style="font-family: var(--font-display); font-size: 11px; display: flex; align-items: center; gap: 6px; cursor: pointer;">
            <input type="checkbox" id="debug-auto-poll" checked>
            <span>Auto Poll (2s)</span>
          </label>
          <button class="btn btn-sm btn-lava" id="btn-flash-overlay">
            <span>FLASH OVERLAY (5s RED SCREEN)</span>
          </button>
          <button class="btn btn-sm" id="btn-debug-refresh">Refresh Debug</button>
        </div>
      </div>

      <div style="display: grid; grid-template-columns: repeat(auto-fit, minmax(440px, 1fr)); gap: 16px;">
        <!-- Minecraft State Card -->
        <div class="debug-card">
          <div class="debug-card-title">
            <span>MINECRAFT CLIENT (GET /api/debug/mc)</span>
          </div>
          <table class="debug-table">
            <tbody id="table-debug-mc">
              <tr><th>JNI</th><td id="mc-jni">--</td></tr>
              <tr><th>MC Ready</th><td id="mc-ready">--</td></tr>
              <tr><th>Extra Hook</th><td id="mc-extra">--</td></tr>
              <tr><th>Naming Mode</th><td id="mc-naming">--</td></tr>
              <tr><th>In Game</th><td id="mc-ingame">--</td></tr>
              <tr><th>Has World</th><td id="mc-hasworld">--</td></tr>
              <tr><th>Screen Open</th><td id="mc-screen">--</td></tr>
              <tr><th>Entity Count</th><td id="mc-entities">--</td></tr>
              <tr><th>Player [X,Y,Z]</th><td id="mc-player">--</td></tr>
              <tr><th>Display [W,H]</th><td id="mc-display">--</td></tr>
              <tr><th>Gamma</th><td id="mc-gamma">--</td></tr>
            </tbody>
          </table>
        </div>

        <!-- ESP & Overlay Card -->
        <div class="debug-card">
          <div class="debug-card-title">
            <span>ESP & OVERLAY (GET /api/debug/esp)</span>
          </div>
          <table class="debug-table">
            <tbody id="table-debug-esp">
              <tr><th>Camera Eye</th><td id="esp-eye">--</td></tr>
              <tr><th>Yaw / Pitch / FOV</th><td id="esp-cam">--</td></tr>
              <tr><th>Boxes Count</th><td id="esp-boxes">--</td></tr>
              <tr><th>Overlay Running</th><td id="esp-running">--</td></tr>
              <tr><th>Window Valid</th><td id="esp-winvalid">--</td></tr>
              <tr><th>Min / Fullscreen</th><td id="esp-minfull">--</td></tr>
              <tr><th>Window Rect</th><td id="esp-rect">--</td></tr>
              <tr><th>DPI</th><td id="esp-dpi">--</td></tr>
              <tr><th>Heartbeats / Lines</th><td id="esp-beats">--</td></tr>
              <tr><th>ULW / Surface Errs</th><td id="esp-errs">--</td></tr>
            </tbody>
          </table>
        </div>
      </div>

      <!-- Raw JSON Inspector -->
      <div class="debug-card">
        <div class="debug-card-title">
          <span>RAW DIAGNOSTIC JSON</span>
        </div>
        <div class="debug-json-box" id="debug-raw-json">Loading debug data...</div>
      </div>
    </div>

  </main>

  <!-- Import Modal Summary -->
  <div class="modal-backdrop" id="import-modal">
    <div class="modal-dialog">
      <div class="modal-header">
        <span>CONFIG IMPORT SUMMARY</span>
      </div>
      <div class="modal-body" id="import-summary-text">
        Processing config file...
      </div>
      <div class="modal-footer">
        <button class="btn btn-primary" id="btn-close-modal">Dismiss</button>
      </div>
    </div>
  </div>

  <!-- Toast Notification Container -->
  <div class="toast-container" id="toast-container"></div>

  <!-- Application Logic -->
  <script>
    (function () {
      "use strict";

      // Detect API base URL
      var API_BASE = (window.location.hostname === "127.0.0.1" && window.location.port === "8080") ? "" : "http://127.0.0.1:8080";

      // Application State
      var state = {
        isLive: false,
        activeCategory: "All",
        searchQuery: "",
        stateFilter: "all",
        expandedModules: {},
        recordingBindModule: null,
        status: {
          name: "a05777Client",
          game: "1.8.9",
          running: true,
          modules: 0,
          enabled: 0,
          frames: 0
        },
        modules: [],
        debugVisible: false,
        debugData: { mc: null, esp: null }
      };

      // Built-in Mock Data for offline preview
      var MOCK_MODULES = [
        {
          name: "HUD",
          category: "Render",
          enabled: true,
          bind: 0,
          bools: [
            { name: "Alphabetical sort", value: false },
            { name: "Draw background", value: false },
            { name: "Text shadow", value: true },
            { name: "Align right", value: true },
            { name: "Lowercase", value: false }
          ],
          sliders: [
            { name: "Scale", value: 1.0, min: 0.5, max: 2.0, step: 0.05 },
            { name: "Pos X", value: 8, min: 0, max: 1920, step: 1 },
            { name: "Pos Y", value: 70, min: 0, max: 1080, step: 1 }
          ],
          modes: [
            { name: "Outline", index: 0, current: "None", options: ["None", "Side", "Full"] }
          ],
          colors: [
            { name: "Color", r: 255, g: 255, b: 255, hex: "#ffffff" }
          ]
        },
        {
          name: "ESP",
          category: "Render",
          enabled: true,
          bind: 70, // F
          bools: [{ name: "Players", value: true }, { name: "Monsters", value: false }, { name: "Passives", value: false }],
          sliders: [
            { name: "Line Width", value: 1.5, min: 1.0, max: 4.0, step: 0.1 }
          ],
          modes: [
            { name: "Style", index: 0, current: "2D Box", options: ["2D Box", "3D Box", "Corners"] }
          ],
          colors: [
            { name: "Box Color", r: 88, g: 190, b: 232, hex: "#58bee8" }
          ]
        },
        {
          name: "Fullbright",
          category: "Render",
          enabled: true,
          bind: 0,
          bools: [{ name: "Night Vision Effect", value: false }],
          sliders: [
            { name: "Brightness", value: 10.0, min: 1.0, max: 15.0, step: 0.5 }
          ],
          modes: [],
          colors: []
        },
        {
          name: "KillAura",
          category: "Combat",
          enabled: false,
          bind: 82, // R
          bools: [{ name: "Raytrace", value: true }, { name: "Lock View", value: false }, { name: "Attack Mobs", value: false }],
          sliders: [
            { name: "Range", value: 4.2, min: 3.0, max: 6.0, step: 0.1 },
            { name: "APS", value: 12.0, min: 5.0, max: 20.0, step: 0.5 }
          ],
          modes: [
            { name: "Target Mode", index: 0, current: "Single", options: ["Single", "Switch", "Multi"] },
            { name: "Priority", index: 1, current: "Distance", options: ["Health", "Distance", "Armor"] }
          ],
          colors: []
        },
        {
          name: "Velocity",
          category: "Combat",
          enabled: false,
          bind: 0,
          bools: [{ name: "Only While Moving", value: false }],
          sliders: [
            { name: "Horizontal", value: 0.0, min: 0.0, max: 100.0, step: 1.0 },
            { name: "Vertical", value: 100.0, min: 0.0, max: 100.0, step: 1.0 },
            { name: "Chance", value: 100.0, min: 0.0, max: 100.0, step: 5.0 }
          ],
          modes: [],
          colors: []
        },
        {
          name: "Auto Block",
          category: "Combat",
          enabled: false,
          bind: 0,
          bools: [
            { name: "Prevent delaying attacks", value: true },
            { name: "Block again immediately", value: true },
            { name: "Force block animation", value: true },
            { name: "Require Left mouse", value: true },
            { name: "Require right mouse", value: false },
            { name: "Damaged", value: false },
            { name: "Ignore teammates", value: true }
          ],
          sliders: [
            { name: "Range", value: 4.0, min: 2.0, max: 6.0, step: 0.1 },
            { name: "Maximum Hurt Time", value: 200.0, min: 50.0, max: 500.0, step: 50.0 },
            { name: "Maximum Hold Time", value: 150.0, min: 50.0, max: 500.0, step: 50.0 },
            { name: "Lag Chance", value: 100.0, min: 0.0, max: 100.0, step: 5.0 },
            { name: "Lag Max Duration", value: 200.0, min: 50.0, max: 500.0, step: 50.0 }
          ],
          modes: [],
          colors: []
        },
        {
          name: "Sprint",
          category: "Movement",
          enabled: true,
          bind: 0,
          bools: [{ name: "Keep Sprint", value: true }],
          sliders: [],
          modes: [
            { name: "Mode", index: 0, current: "Legit", options: ["Legit", "Omni"] }
          ],
          colors: []
        },
        {
          name: "Speed",
          category: "Movement",
          enabled: false,
          bind: 0,
          bools: [{ name: "Damage Boost", value: false }],
          sliders: [
            { name: "Speed Factor", value: 1.25, min: 1.0, max: 3.0, step: 0.05 }
          ],
          modes: [
            { name: "Mode", index: 1, current: "Hop", options: ["Vanilla", "Hop", "NCP"] }
          ],
          colors: []
        },
        {
          name: "FastPlace",
          category: "Player",
          enabled: false,
          bind: 0,
          bools: [{ name: "Blocks Only", value: true }],
          sliders: [
            { name: "Click Delay", value: 0, min: 0, max: 4, step: 1 }
          ],
          modes: [],
          colors: []
        },
        {
          name: "NoFall",
          category: "Player",
          enabled: false,
          bind: 0,
          bools: [],
          sliders: [
            { name: "Min Fall Distance", value: 3.0, min: 2.0, max: 10.0, step: 0.5 }
          ],
          modes: [
            { name: "Mode", index: 0, current: "Packet", options: ["Packet", "Catch", "Damage"] }
          ],
          colors: []
        },
        {
          name: "Scaffold",
          category: "World",
          enabled: false,
          bind: 86, // V
          bools: [{ name: "Tower", value: true }, { name: "Keep Y", value: false }, { name: "Safewalk", value: true }],
          sliders: [
            { name: "Expand", value: 0, min: 0, max: 3, step: 1 }
          ],
          modes: [
            { name: "Mode", index: 0, current: "Normal", options: ["Normal", "Breezily", "Godbridge"] }
          ],
          colors: []
        },
        {
          name: "BedAura",
          category: "Minigames",
          enabled: false,
          bind: 0,
          bools: [{ name: "Through Walls", value: true }],
          sliders: [
            { name: "Range", value: 4.5, min: 2.0, max: 6.0, step: 0.1 }
          ],
          modes: [],
          colors: []
        },
        {
          name: "DiscordRPC",
          category: "Other",
          enabled: false,
          bind: 0,
          bools: [{ name: "Show Server", value: true }, { name: "Show Health", value: false }],
          sliders: [],
          modes: [],
          colors: []
        },
        {
          name: "ClickGUI",
          category: "Client",
          enabled: false,
          bind: 54, // RSHIFT
          bools: [{ name: "Sound Effects", value: true }],
          sliders: [
            { name: "Scale", value: 1.0, min: 0.8, max: 1.4, step: 0.05 }
          ],
          modes: [
            { name: "Theme", index: 0, current: "Tactical", options: ["Tactical", "Dark", "Forest"] }
          ],
          colors: [
            { name: "Accent", r: 157, g: 222, b: 74, hex: "#9dde4a" }
          ]
        }
      ];

      // Key Code standard mapping
      var KEY_NAMES = {
        0: "NONE",
        8: "BACKSPACE",
        9: "TAB",
        13: "ENTER",
        16: "SHIFT",
        17: "CTRL",
        18: "ALT",
        19: "PAUSE",
        20: "CAPSLOCK",
        27: "ESC",
        32: "SPACE",
        33: "PGUP",
        34: "PGDN",
        35: "END",
        36: "HOME",
        37: "LEFT",
        38: "UP",
        39: "RIGHT",
        40: "DOWN",
        45: "INSERT",
        46: "DELETE",
        54: "RSHIFT"
      };

      function formatKeyName(code) {
        if (!code || code === 0) return "NONE";
        if (KEY_NAMES[code]) return KEY_NAMES[code];
        if (code >= 65 && code <= 90) return String.fromCharCode(code);
        if (code >= 48 && code <= 57) return String.fromCharCode(code);
        if (code >= 112 && code <= 123) return "F" + (code - 111);
        return "KEY " + code;
      }

      // Toast Notifications
      function showToast(message, type) {
        var container = document.getElementById("toast-container");
        var toast = document.createElement("div");
        toast.className = "toast " + (type || "info");
        toast.textContent = message;
        container.appendChild(toast);
        setTimeout(function () {
          toast.style.opacity = "0";
          toast.style.transform = "translateY(10px)";
          toast.style.transition = "all 0.25s ease";
          setTimeout(function () {
            if (toast.parentNode) toast.parentNode.removeChild(toast);
          }, 250);
        }, 3200);
      }

      // Debounce utility
      var debounceTimers = {};
      function debounce(key, fn, delay) {
        if (debounceTimers[key]) clearTimeout(debounceTimers[key]);
        debounceTimers[key] = setTimeout(fn, delay || 140);
      }

      // API Call Wrapper
      function apiCall(endpoint, options) {
        options = options || {};
        var url = API_BASE + endpoint;
        var fetchOpts = {
          method: options.method || "GET",
          headers: {}
        };
        if (options.body) {
          fetchOpts.headers["Content-Type"] = "application/json";
          fetchOpts.body = JSON.stringify(options.body);
        }

        return fetch(url, fetchOpts)
          .then(function (res) {
            if (!res.ok) {
              return res.json().catch(function () { return { error: "HTTP " + res.status }; })
                .then(function (errData) {
                  var err = new Error(errData.error || ("Request failed with status " + res.status));
                  err.status = res.status;
                  throw err;
                });
            }
            return res.json();
          });
      }

      // Status Polling
      function fetchStatus() {
        apiCall("/api/status")
          .then(function (data) {
            state.status = data;
            if (!state.isLive) {
              state.isLive = true;
              document.getElementById("offline-banner").classList.remove("active");
              fetchModules();
            }
            updateHeaderStatus();
          })
          .catch(function () {
            if (state.isLive) {
              state.isLive = false;
            }
            document.getElementById("offline-banner").classList.add("active");
            updateHeaderStatus();
          });
      }

      // Modules Fetching
      function fetchModules() {
        if (!state.isLive) {
          state.modules = JSON.parse(JSON.stringify(MOCK_MODULES));
          renderCategories();
          renderModules();
          updateHeaderStatus();
          return;
        }

        apiCall("/api/modules")
          .then(function (data) {
            state.modules = Array.isArray(data) ? data : [];
            renderCategories();
            renderModules();
            updateHeaderStatus();
          })
          .catch(function (err) {
            showToast("Failed to fetch modules: " + err.message, "error");
          });
      }

      // Update Header Status UI
      function updateHeaderStatus() {
        var dot = document.getElementById("status-dot");
        var conn = document.getElementById("status-conn");
        var stateElem = document.getElementById("status-state");
        var activeElem = document.getElementById("status-active");
        var fpsElem = document.getElementById("status-fps");
        var gameTag = document.getElementById("game-tag");

        if (state.status.game) {
          gameTag.textContent = state.status.game;
        }

        if (state.isLive) {
          dot.className = "live-dot pulse";
          conn.textContent = "CONNECTED";
          conn.className = "status-val green";
        } else {
          dot.className = "live-dot offline";
          conn.textContent = "PREVIEW";
          conn.className = "status-val orange";
        }

        stateElem.textContent = state.status.running ? "RUNNING" : "STOPPED";
        stateElem.className = "status-val " + (state.status.running ? "green" : "orange");

        var enabledCount = 0;
        var totalCount = state.modules.length;
        for (var i = 0; i < totalCount; i++) {
          if (state.modules[i].enabled) enabledCount++;
        }

        activeElem.textContent = enabledCount + " / " + totalCount;
        fpsElem.textContent = (state.status.frames !== undefined ? state.status.frames : "--") + " FPS";
      }

      // Categories setup
      var CATEGORIES = ["All", "Combat", "Movement", "Player", "Render", "World", "Minigames", "Other", "Client"];

      function renderCategories() {
        var wrap = document.getElementById("category-tabs");
        wrap.innerHTML = "";

        // Count modules per category
        var counts = { All: state.modules.length };
        for (var i = 0; i < state.modules.length; i++) {
          var cat = state.modules[i].category || "Other";
          counts[cat] = (counts[cat] || 0) + 1;
        }

        CATEGORIES.forEach(function (cat) {
          var btn = document.createElement("button");
          btn.className = "tab-btn" + (state.activeCategory === cat ? " active" : "");
          btn.innerHTML = cat + " <span class=\"badge\">" + (counts[cat] || 0) + "</span>";
          btn.addEventListener("click", function () {
            state.activeCategory = cat;
            if (state.debugVisible) toggleDebugView(false);
            renderCategories();
            renderModules();
          });
          wrap.appendChild(btn);
        });
      }

      // Filtered modules
      function getFilteredModules() {
        return state.modules.filter(function (m) {
          if (state.activeCategory !== "All" && m.category !== state.activeCategory) {
            return false;
          }
          if (state.stateFilter === "enabled" && !m.enabled) return false;
          if (state.stateFilter === "disabled" && m.enabled) return false;

          if (state.searchQuery) {
            var q = state.searchQuery.toLowerCase();
            var nameMatch = (m.name || "").toLowerCase().indexOf(q) !== -1;
            var settingMatch = false;

            if (m.bools) {
              settingMatch = settingMatch || m.bools.some(function (b) { return b.name.toLowerCase().indexOf(q) !== -1; });
            }
            if (m.sliders) {
              settingMatch = settingMatch || m.sliders.some(function (s) { return s.name.toLowerCase().indexOf(q) !== -1; });
            }
            if (m.modes) {
              settingMatch = settingMatch || m.modes.some(function (md) { return md.name.toLowerCase().indexOf(q) !== -1; });
            }
            if (!nameMatch && !settingMatch) return false;
          }
          return true;
        });
      }

      // Render modules grid
      function renderModules() {
        var grid = document.getElementById("module-grid");
        grid.innerHTML = "";

        var list = getFilteredModules();
        var countIndicator = document.getElementById("count-indicator");
        countIndicator.textContent = "Showing " + list.length + " of " + state.modules.length + " modules";

        if (list.length === 0) {
          var empty = document.createElement("div");
          empty.className = "empty-state";
          empty.textContent = "No modules matched the active filters.";
          grid.appendChild(empty);
          return;
        }

        list.forEach(function (m) {
          var card = buildModuleCard(m);
          grid.appendChild(card);
        });
      }

      // Build single card element
      function buildModuleCard(mod) {
        var card = document.createElement("div");
        var hasSettings = (mod.bools && mod.bools.length > 0) ||
                           (mod.sliders && mod.sliders.length > 0) ||
                           (mod.modes && mod.modes.length > 0) ||
                           (mod.colors && mod.colors.length > 0);

        var isExpanded = !!state.expandedModules[mod.name];
        card.className = "module-card" +
          (mod.enabled ? " is-enabled" : "") +
          (hasSettings ? " has-settings" : "") +
          (isExpanded ? " is-expanded" : "");

        // Card Header
        var header = document.createElement("div");
        header.className = "card-header";

        var left = document.createElement("div");
        left.className = "header-left";

        var title = document.createElement("span");
        title.className = "module-name";
        title.textContent = mod.name;
        title.title = mod.name;

        var catBadge = document.createElement("span");
        catBadge.className = "category-badge";
        catBadge.textContent = mod.category || "General";

        left.appendChild(title);
        left.appendChild(catBadge);

        var right = document.createElement("div");
        right.className = "header-right";

        // Keybind Pill
        var bindBtn = document.createElement("button");
        var isRec = state.recordingBindModule === mod.name;
        bindBtn.className = "keybind-btn" + (isRec ? " recording" : "");
        bindBtn.textContent = isRec ? "PRESS KEY..." : formatKeyName(mod.bind);
        bindBtn.title = "Click to set keybind (Esc to clear)";
        bindBtn.addEventListener("click", function (e) {
          e.stopPropagation();
          if (state.recordingBindModule === mod.name) {
            state.recordingBindModule = null;
          } else {
            state.recordingBindModule = mod.name;
          }
          renderModules();
        });
        right.appendChild(bindBtn);

        // Toggle Switch
        var toggleLabel = document.createElement("label");
        toggleLabel.className = "toggle-switch";
        toggleLabel.title = "Toggle " + mod.name;
        toggleLabel.addEventListener("click", function (e) { e.stopPropagation(); });

        var toggleInput = document.createElement("input");
        toggleInput.type = "checkbox";
        toggleInput.checked = !!mod.enabled;
        toggleInput.addEventListener("change", function () {
          toggleModule(mod.name, toggleInput.checked);
        });

        var toggleTrack = document.createElement("span");
        toggleTrack.className = "toggle-track";

        toggleLabel.appendChild(toggleInput);
        toggleLabel.appendChild(toggleTrack);
        right.appendChild(toggleLabel);

        // Expand Chevron
        if (hasSettings) {
          var chevron = document.createElement("span");
          chevron.className = "expand-icon";
          chevron.textContent = ">";
          right.appendChild(chevron);

          header.addEventListener("click", function () {
            state.expandedModules[mod.name] = !state.expandedModules[mod.name];
            renderModules();
          });
        }

        header.appendChild(left);
        header.appendChild(right);
        card.appendChild(header);

        // Card Body (Settings)
        if (hasSettings) {
          var body = document.createElement("div");
          body.className = "card-body";

          // Bools
          if (mod.bools && mod.bools.length > 0) {
            var bgGroup = document.createElement("div");
            bgGroup.className = "setting-group";
            var bgTitle = document.createElement("div");
            bgTitle.className = "setting-group-title";
            bgTitle.textContent = "TOGGLES";
            bgGroup.appendChild(bgTitle);

            mod.bools.forEach(function (b) {
              var boolRow = document.createElement("div");
              boolRow.className = "setting-bool";

              var label = document.createElement("span");
              label.className = "setting-bool-label";
              label.textContent = b.name;

              var swLabel = document.createElement("label");
              swLabel.className = "toggle-switch";

              var swInput = document.createElement("input");
              swInput.type = "checkbox";
              swInput.checked = !!b.value;
              swInput.addEventListener("change", function () {
                b.value = swInput.checked;
                var payload = { bools: {} };
                payload.bools[b.name] = b.value;
                updateModuleSettings(mod.name, payload);
              });

              var swTrack = document.createElement("span");
              swTrack.className = "toggle-track";
              swLabel.appendChild(swInput);
              swLabel.appendChild(swTrack);

              boolRow.appendChild(label);
              boolRow.appendChild(swLabel);
              bgGroup.appendChild(boolRow);
            });
            body.appendChild(bgGroup);
          }

          // Sliders
          if (mod.sliders && mod.sliders.length > 0) {
            var slGroup = document.createElement("div");
            slGroup.className = "setting-group";
            var slTitle = document.createElement("div");
            slTitle.className = "setting-group-title";
            slTitle.textContent = "PARAMETERS";
            slGroup.appendChild(slTitle);

            mod.sliders.forEach(function (s) {
              var slRow = document.createElement("div");
              slRow.className = "setting-slider";

              var head = document.createElement("div");
              head.className = "slider-header";

              var sName = document.createElement("span");
              sName.className = "slider-name";
              sName.textContent = s.name;

              var sBadge = document.createElement("span");
              sBadge.className = "slider-badge";
              sBadge.textContent = Number(s.value).toFixed(getDecimals(s.step));

              head.appendChild(sName);
              head.appendChild(sBadge);
              slRow.appendChild(head);

              var rangeWrap = document.createElement("div");
              rangeWrap.className = "slider-range-wrap";

              var minSpan = document.createElement("span");
              minSpan.textContent = s.min;

              var maxSpan = document.createElement("span");
              maxSpan.className = "max";
              maxSpan.textContent = s.max;

              var input = document.createElement("input");
              input.type = "range";
              input.min = s.min;
              input.max = s.max;
              input.step = s.step || 0.1;
              input.value = s.value;

              input.addEventListener("input", function () {
                var val = parseFloat(input.value);
                s.value = val;
                sBadge.textContent = Number(val).toFixed(getDecimals(s.step));

                debounce("slider_" + mod.name + "_" + s.name, function () {
                  var payload = { sliders: {} };
                  payload.sliders[s.name] = val;
                  updateModuleSettings(mod.name, payload);
                }, 120);
              });

              // Allow direct edit on badge click
              sBadge.addEventListener("click", function () {
                var current = s.value;
                var res = window.prompt("Enter value for " + s.name + " (" + s.min + " - " + s.max + "):", current);
                if (res !== null) {
                  var parsed = parseFloat(res);
                  if (!isNaN(parsed)) {
                    parsed = Math.max(s.min, Math.min(s.max, parsed));
                    s.value = parsed;
                    input.value = parsed;
                    sBadge.textContent = Number(parsed).toFixed(getDecimals(s.step));
                    var payload = { sliders: {} };
                    payload.sliders[s.name] = parsed;
                    updateModuleSettings(mod.name, payload);
                  }
                }
              });

              rangeWrap.appendChild(minSpan);
              rangeWrap.appendChild(input);
              rangeWrap.appendChild(maxSpan);

              slRow.appendChild(rangeWrap);
              slGroup.appendChild(slRow);
            });
            body.appendChild(slGroup);
          }

          // Modes
          if (mod.modes && mod.modes.length > 0) {
            var mdGroup = document.createElement("div");
            mdGroup.className = "setting-group";
            var mdTitle = document.createElement("div");
            mdTitle.className = "setting-group-title";
            mdTitle.textContent = "MODES";
            mdGroup.appendChild(mdTitle);

            mod.modes.forEach(function (md) {
              var mdRow = document.createElement("div");
              mdRow.className = "setting-mode";

              var mHead = document.createElement("div");
              mHead.className = "mode-header";
              mHead.textContent = md.name;
              mdRow.appendChild(mHead);

              var optRow = document.createElement("div");
              optRow.className = "mode-options-row";

              var opts = md.options || [];
              opts.forEach(function (opt, idx) {
                var mBtn = document.createElement("button");
                var isSelected = (typeof md.index === "number" && md.index === idx) || md.current === opt;
                mBtn.className = "mode-btn" + (isSelected ? " active" : "");
                mBtn.textContent = opt;

                mBtn.addEventListener("click", function () {
                  md.index = idx;
                  md.current = opt;
                  var payload = { modes: {} };
                  payload.modes[md.name] = idx;
                  updateModuleSettings(mod.name, payload);
                  renderModules();
                });

                optRow.appendChild(mBtn);
              });

              mdRow.appendChild(optRow);
              mdGroup.appendChild(mdRow);
            });
            body.appendChild(mdGroup);
          }

          // Colors
          if (mod.colors && mod.colors.length > 0) {
            var colGroup = document.createElement("div");
            colGroup.className = "setting-group";
            var colTitle = document.createElement("div");
            colTitle.className = "setting-group-title";
            colTitle.textContent = "COLORS";
            colGroup.appendChild(colTitle);

            mod.colors.forEach(function (c) {
              var colRow = document.createElement("div");
              colRow.className = "setting-color";

              var cLabel = document.createElement("span");
              cLabel.className = "color-label";
              cLabel.textContent = c.name;

              var pickWrap = document.createElement("div");
              pickWrap.className = "color-picker-wrap";

              var preview = document.createElement("div");
              preview.className = "color-preview";
              preview.style.backgroundColor = c.hex;

              var hexSpan = document.createElement("span");
              hexSpan.className = "color-hex";
              hexSpan.textContent = c.hex;

              var hiddenPicker = document.createElement("input");
              hiddenPicker.type = "color";
              hiddenPicker.value = c.hex;

              preview.addEventListener("click", function () {
                hiddenPicker.click();
              });

              hiddenPicker.addEventListener("input", function () {
                var hex = hiddenPicker.value;
                c.hex = hex;
                preview.style.backgroundColor = hex;
                hexSpan.textContent = hex;

                debounce("color_" + mod.name + "_" + c.name, function () {
                  var payload = { colors: {} };
                  payload.colors[c.name] = hex;
                  updateModuleSettings(mod.name, payload);
                }, 100);
              });

              pickWrap.appendChild(preview);
              pickWrap.appendChild(hexSpan);
              pickWrap.appendChild(hiddenPicker);

              colRow.appendChild(cLabel);
              colRow.appendChild(pickWrap);
              colGroup.appendChild(colRow);
            });
            body.appendChild(colGroup);
          }

          card.appendChild(body);
        }

        return card;
      }

      function getDecimals(step) {
        if (!step) return 2;
        var s = String(step);
        var dot = s.indexOf(".");
        return dot === -1 ? 0 : s.length - dot - 1;
      }

      // Action: Toggle Module
      function toggleModule(name, newState) {
        var mod = state.modules.find(function (m) { return m.name === name; });
        if (mod) mod.enabled = newState;
        updateHeaderStatus();
        renderModules();

        if (!state.isLive) {
          showToast(name + (newState ? " enabled" : " disabled") + " (Simulated)", "info");
          return;
        }

        apiCall("/api/modules/" + encodeURIComponent(name) + "/toggle", { method: "POST" })
          .then(function (res) {
            if (mod && res) {
              mod.enabled = res.enabled;
            }
            updateHeaderStatus();
            renderModules();
            showToast(name + (res.enabled ? " enabled" : " disabled"), "success");
          })
          .catch(function (err) {
            showToast("Toggle " + name + " failed: " + err.message, "error");
            if (mod) mod.enabled = !newState;
            updateHeaderStatus();
            renderModules();
          });
      }

      // Action: PUT Module Settings
      function updateModuleSettings(name, payload) {
        if (!state.isLive) {
          showToast("Saved " + name + " (Simulated)", "info");
          return;
        }

        apiCall("/api/modules/" + encodeURIComponent(name), {
          method: "PUT",
          body: payload
        })
          .then(function (updated) {
            var idx = state.modules.findIndex(function (m) { return m.name === name; });
            if (idx !== -1 && updated) {
              state.modules[idx] = updated;
            }
          })
          .catch(function (err) {
            showToast("Update " + name + " failed: " + err.message, "error");
          });
      }

      // Keybind Global Listener
      window.addEventListener("keydown", function (e) {
        if (!state.recordingBindModule) return;

        e.preventDefault();
        e.stopPropagation();

        var modName = state.recordingBindModule;
        state.recordingBindModule = null;

        var keyCode = e.keyCode || e.which;
        if (keyCode === 27) { // Escape clears bind
          keyCode = 0;
        }

        var mod = state.modules.find(function (m) { return m.name === modName; });
        if (mod) {
          mod.bind = keyCode;
        }

        updateModuleSettings(modName, { bind: keyCode });
        renderModules();
        showToast(modName + " bind set to " + formatKeyName(keyCode), "success");
      });

      // Export Config
      function exportConfig() {
        var config = {
          app: "a05777Client",
          version: 1,
          modules: {}
        };

        state.modules.forEach(function (m) {
          var boolsMap = {};
          (m.bools || []).forEach(function (b) { boolsMap[b.name] = b.value; });

          var slidersMap = {};
          (m.sliders || []).forEach(function (s) { slidersMap[s.name] = s.value; });

          var modesMap = {};
          (m.modes || []).forEach(function (md) {
            modesMap[md.name] = (typeof md.index === "number" ? md.index : 0);
          });

          var colorsMap = {};
          (m.colors || []).forEach(function (c) { colorsMap[c.name] = c.hex; });

          config.modules[m.name] = {
            enabled: !!m.enabled,
            bind: m.bind || 0,
            bools: boolsMap,
            sliders: slidersMap,
            modes: modesMap,
            colors: colorsMap
          };
        });

        var jsonStr = JSON.stringify(config, null, 2);
        var blob = new Blob([jsonStr], { type: "application/json" });
        var url = URL.createObjectURL(blob);
        var a = document.createElement("a");
        var dateStr = new Date().toISOString().slice(0, 10);
        a.href = url;
        a.download = "a05777Client_config_" + dateStr + ".json";
        document.body.appendChild(a);
        a.click();
        document.body.removeChild(a);
        URL.revokeObjectURL(url);
        showToast("Config exported successfully", "success");
      }

      // Import Config
      function handleFileImport(e) {
        var file = e.target.files && e.target.files[0];
        if (!file) return;

        var reader = new FileReader();
        reader.onload = function (evt) {
          try {
            var data = JSON.parse(evt.target.result);
            if (!data || data.app !== "a05777Client" || !data.modules) {
              showToast("Invalid config format: app identifier missing", "error");
              return;
            }
            processConfigImport(data.modules);
          } catch (err) {
            showToast("Failed to parse JSON file: " + err.message, "error");
          }
        };
        reader.readAsText(file);
        e.target.value = ""; // Reset
      }

      function processConfigImport(modConfigs) {
        var moduleKeys = Object.keys(modConfigs);
        var total = moduleKeys.length;
        var applied = 0;
        var skipped = 0;

        if (!state.isLive) {
          moduleKeys.forEach(function (k) {
            var target = state.modules.find(function (m) { return m.name === k; });
            if (target) {
              var c = modConfigs[k];
              if (c.enabled !== undefined) target.enabled = c.enabled;
              if (c.bind !== undefined) target.bind = c.bind;
              if (c.bools && target.bools) {
                target.bools.forEach(function (b) { if (c.bools[b.name] !== undefined) b.value = c.bools[b.name]; });
              }
              if (c.sliders && target.sliders) {
                target.sliders.forEach(function (s) { if (c.sliders[s.name] !== undefined) s.value = c.sliders[s.name]; });
              }
              if (c.modes && target.modes) {
                target.modes.forEach(function (md) {
                  if (c.modes[md.name] !== undefined) {
                    var val = c.modes[md.name];
                    if (typeof val === "number") {
                      md.index = val;
                      if (md.options && md.options[val]) md.current = md.options[val];
                    }
                  }
                });
              }
              if (c.colors && target.colors) {
                target.colors.forEach(function (col) { if (c.colors[col.name] !== undefined) col.hex = c.colors[col.name]; });
              }
              applied++;
            } else {
              skipped++;
            }
          });

          renderModules();
          updateHeaderStatus();
          openImportSummary(applied, skipped, total);
          return;
        }

        // Live API batch import
        var promises = moduleKeys.map(function (modName) {
          var payload = modConfigs[modName];
          return apiCall("/api/modules/" + encodeURIComponent(modName), {
            method: "PUT",
            body: payload
          })
            .then(function () { applied++; })
            .catch(function () { skipped++; });
        });

        Promise.all(promises).then(function () {
          fetchModules();
          openImportSummary(applied, skipped, total);
        });
      }

      function openImportSummary(applied, skipped, total) {
        var modal = document.getElementById("import-modal");
        var text = document.getElementById("import-summary-text");
        text.innerHTML = "Import process completed.<br><br>" +
          "<strong>Total entries in file:</strong> " + total + "<br>" +
          "<strong>Successfully applied:</strong> " + applied + "<br>" +
          "<strong>Skipped (unknown or missing modules):</strong> " + skipped;
        modal.classList.add("open");
      }

      // Debug View & Polling
      function toggleDebugView(force) {
        state.debugVisible = (force !== undefined) ? force : !state.debugVisible;
        var section = document.getElementById("debug-section");
        var grid = document.getElementById("module-grid");
        var navBar = document.querySelector(".nav-bar");
        var btn = document.getElementById("btn-toggle-debug");

        if (state.debugVisible) {
          section.classList.add("active");
          grid.style.display = "none";
          navBar.style.display = "none";
          btn.style.borderColor = "var(--grass)";
          btn.style.color = "var(--grass)";
          fetchDebugData();
        } else {
          section.classList.remove("active");
          grid.style.display = "grid";
          navBar.style.display = "flex";
          btn.style.borderColor = "";
          btn.style.color = "";
        }
      }

      function fetchDebugData() {
        if (!state.debugVisible) return;

        if (!state.isLive) {
          // Mock debug data
          var mockMc = {
            jni: true,
            mcReady: true,
            mcExtra: true,
            naming: "notch/searge",
            inGame: true,
            hasWorld: true,
            screenOpen: false,
            entities: 52,
            player: [-142.45, 64.0, 280.12],
            display: [1920, 1080],
            gamma: 15.0
          };
          var mockEsp = {
            camera: { eye: [-142.45, 65.62, 280.12], yaw: 88.5, pitch: -4.2, fov: 70.0, wh: [1920, 1080] },
            boxes: [{ x: -140, y: 64, w: 0.6, h: 1.8, hp: 20 }],
            overlay: {
              running: true,
              hasWindow: true,
              windowValid: true,
              minimized: false,
              fullscreen: false,
              rect: [0, 0, 1920, 1080],
              dpi: 96,
              ulwFailed: 0,
              ulwErr: 0,
              surfFailed: 0,
              beats: 3412,
              lines: 48,
              iters: 1024,
              stage: 3,
              flash: false,
              paint: true,
              win: 1
            }
          };
          renderDebugTables(mockMc, mockEsp);
          return;
        }

        Promise.all([
          apiCall("/api/debug/mc").catch(function () { return null; }),
          apiCall("/api/debug/esp").catch(function () { return null; })
        ]).then(function (results) {
          renderDebugTables(results[0], results[1]);
        });
      }

      function renderDebugTables(mc, esp) {
        state.debugData = { mc: mc, esp: esp };

        if (mc) {
          document.getElementById("mc-jni").textContent = String(mc.jni);
          document.getElementById("mc-ready").textContent = String(mc.mcReady);
          document.getElementById("mc-extra").textContent = String(mc.mcExtra);
          document.getElementById("mc-naming").textContent = mc.naming || "--";
          document.getElementById("mc-ingame").textContent = String(mc.inGame);
          document.getElementById("mc-hasworld").textContent = String(mc.hasWorld);
          document.getElementById("mc-screen").textContent = String(mc.screenOpen);
          document.getElementById("mc-entities").textContent = String(mc.entities);
          document.getElementById("mc-player").textContent = mc.player ? ("[" + mc.player.map(function(n){ return Number(n).toFixed(2); }).join(", ") + "]") : "--";
          document.getElementById("mc-display").textContent = mc.display ? (mc.display[0] + " x " + mc.display[1]) : "--";
          document.getElementById("mc-gamma").textContent = String(mc.gamma);
        }

        if (esp) {
          var cam = esp.camera || {};
          var ovr = esp.overlay || {};
          document.getElementById("esp-eye").textContent = cam.eye ? ("[" + cam.eye.map(function(n){ return Number(n).toFixed(2); }).join(", ") + "]") : "--";
          document.getElementById("esp-cam").textContent = (cam.yaw !== undefined ? cam.yaw : "--") + " / " + (cam.pitch !== undefined ? cam.pitch : "--") + " / " + (cam.fov !== undefined ? cam.fov : "--");
          document.getElementById("esp-boxes").textContent = esp.boxes ? (esp.boxes.length + " targets") : "--";
          document.getElementById("esp-running").textContent = String(ovr.running);
          document.getElementById("esp-winvalid").textContent = String(ovr.windowValid);
          document.getElementById("esp-minfull").textContent = "Min: " + ovr.minimized + " / Full: " + ovr.fullscreen;
          document.getElementById("esp-rect").textContent = ovr.rect ? ("[" + ovr.rect.join(", ") + "]") : "--";
          document.getElementById("esp-dpi").textContent = String(ovr.dpi);
          document.getElementById("esp-beats").textContent = "Beats: " + (ovr.beats || 0) + " / Lines: " + (ovr.lines || 0);
          document.getElementById("esp-errs").textContent = "ULW: " + (ovr.ulwFailed || 0) + " / Surf: " + (ovr.surfFailed || 0);
        }

        document.getElementById("debug-raw-json").textContent = JSON.stringify({ mc: mc, esp: esp }, null, 2);
      }

      // Flash Overlay Test
      function flashOverlay() {
        var btn = document.getElementById("btn-flash-overlay");
        btn.disabled = true;
        var count = 5;
        btn.textContent = "FLASHING (" + count + "s)...";

        var interval = setInterval(function () {
          count--;
          if (count > 0) {
            btn.textContent = "FLASHING (" + count + "s)...";
          } else {
            clearInterval(interval);
            btn.disabled = false;
            btn.textContent = "FLASH OVERLAY (5s RED SCREEN)";
          }
        }, 1000);

        if (!state.isLive) {
          showToast("Flash overlay triggered (5s Simulated Red Screen)", "info");
          return;
        }

        apiCall("/api/debug/overlay/flash", { method: "POST" })
          .then(function () {
            showToast("Overlay flash active for 5 seconds", "success");
          })
          .catch(function (err) {
            showToast("Flash overlay failed: " + err.message, "error");
          });
      }

      // Event Listeners Wiring
      document.getElementById("btn-refresh").addEventListener("click", function () {
        showToast("Refreshing status & modules...", "info");
        fetchStatus();
        fetchModules();
      });

      document.getElementById("btn-export").addEventListener("click", exportConfig);
      document.getElementById("file-import").addEventListener("change", handleFileImport);
      document.getElementById("btn-toggle-debug").addEventListener("click", function () { toggleDebugView(); });
      document.getElementById("btn-debug-refresh").addEventListener("click", fetchDebugData);
      document.getElementById("btn-flash-overlay").addEventListener("click", flashOverlay);
      document.getElementById("banner-retry").addEventListener("click", function () {
        fetchStatus();
        fetchModules();
      });

      document.getElementById("btn-close-modal").addEventListener("click", function () {
        document.getElementById("import-modal").classList.remove("open");
      });

      document.getElementById("btn-expand-all").addEventListener("click", function () {
        state.modules.forEach(function (m) { state.expandedModules[m.name] = true; });
        renderModules();
      });

      document.getElementById("btn-collapse-all").addEventListener("click", function () {
        state.expandedModules = {};
        renderModules();
      });

      document.getElementById("search-input").addEventListener("input", function (e) {
        state.searchQuery = e.target.value;
        renderModules();
      });

      // Shortcut '/' to focus search
      window.addEventListener("keyup", function (e) {
        if (e.key === "/" && document.activeElement !== document.getElementById("search-input")) {
          e.preventDefault();
          document.getElementById("search-input").focus();
        }
      });

      document.getElementById("filter-state").addEventListener("change", function (e) {
        state.stateFilter = e.target.value;
        renderModules();
      });

      // Timers: Status Polling every 2s
      setInterval(function () {
        fetchStatus();
        if (state.debugVisible && document.getElementById("debug-auto-poll").checked) {
          fetchDebugData();
        }
      }, 2000);

      // Initial Kickoff
      fetchStatus();
      fetchModules();
    })();
  </script>
</body>
</html>)WEB";

}  // namespace web
}  // namespace raven
