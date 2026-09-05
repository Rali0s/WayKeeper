# WayKeeper Workstation Research

Checked 2026-08-16. Bloomberg Terminal is a proprietary product and trademark of Bloomberg
Finance L.P. WayKeeper borrows general workstation interaction patterns—not Bloomberg code,
data, commands, branding, or screen reproductions.

## What is technically supportable

- Bloomberg's current API documentation exposes matching event-driven interfaces for C++, Java,
  C#, and Python. The native C/C++ API describes asynchronous sessions, services, requests,
  subscriptions, events, and messages. This validates C++ as an appropriate language for a
  low-latency workstation core, but it does not reveal the proprietary Terminal GUI implementation.
- Bloomberg's current API library page says newer language bindings use a native C++ backend
  through foreign-function interfaces. That is useful evidence for keeping WayKeeper's state,
  input, indexing, and telemetry layers native and exposing optional adapters at their boundary.
- Bloomberg's own engineering material says the Terminal evolved to an embedded web-browser stack
  and uses JavaScript/TypeScript applications with Chromium and V8. The accurate modern phrase is
  **embedded JavaScript**, not **embedded Java**. Bloomberg separately offers a Java API, but that
  does not establish that the Terminal embeds a JVM.
- Accounts of the early server describe substantial Fortran and C use. This is historical context,
  not a reason to add a Fortran runtime to WayKeeper. Numerical kernels could be isolated behind a
  C ABI later if a measured need appears; the Phase-A interface remains C++20 only.

## Interaction patterns worth adapting

Bloomberg's own education material explicitly teaches navigation through the keyboard, command
line, tabs, menus, and autocomplete. Its product material describes customizable Launchpad
workspaces combining monitors, alerts, charts, and news. Bloomberg support also documents the
command-plus-action-key convention and persistent function keys.

WayKeeper adapts those general patterns as follows:

| Workstation pattern | WayKeeper adaptation |
|---|---|
| Command line plus action key | Existing `:` commands and Enter-to-open behavior |
| Persistent colored keys | Existing F1-F12 operations rail |
| Tabs and menus | Tab/Shift-Tab focus rail above a fixed VIM menu |
| Search/autocomplete field | Local Archive Find field; no network dependency |
| Multiple dense monitors | Menu, companion image, inventory, health, power, and readiness panes |
| Compact charts | Deterministic ANSI readiness/resource bars from local inventory state |
| Custom workspace | Persistent `workstation` or `static` command-center model |

The legacy static command center remains intact for small terminals and lowest-power deployments.
No browser engine, JVM, JavaScript runtime, Bloomberg API, or live market-data dependency is added.

## Primary and authoritative references

- Bloomberg BLPAPI documentation: https://bloomberg.github.io/blpapi-docs/
- Bloomberg API Library: https://professional.bloomberg.com/support/api-library/
- Bloomberg Terminal certificate-course description (keyboard, command line, tabs, menus,
  autocomplete): https://professional.bloomberg.com/products/bloomberg-terminal/education/certificate-courses/
- Bloomberg Terminal product overview (Launchpad monitors, alerts, charting, news):
  https://professional.bloomberg.com/products/bloomberg-terminal/
- Bloomberg customer support (command line, function keys, HELP convention):
  https://professional.bloomberg.com/support/customer-support/
- Bloomberg engineering on the Terminal's embedded Chromium/V8 JavaScript stack:
  https://www.bloomberg.com/company/stories/temporal-is-now-official-transforming-javascript-dates-times-with-bloomberg-support/

The early Fortran/C lineage is described in secondary historical reporting and should be labeled
as such whenever cited; Bloomberg does not publish the proprietary Terminal source architecture.
