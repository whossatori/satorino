# Stage F batch 8

BATCH7_HEAD: 0b329736eef878f2e356d3d671b93b537f63e55b
Base tree: aa5b3e1ec4199ef7330c3231f3bf6398eada7717
Setup: fetched origin; origin/satorino equals BATCH7_HEAD; origin/satorino..stage-f empty; initial working tree clean.

## B0 research (recorded before feature implementation)

- [Twitch channel.raid](https://dev.twitch.tv/docs/eventsub/eventsub-subscription-types/#channelraid): version 1 requires no event-specific authorization. Conditions choose either from_broadcaster_user_id or to_broadcaster_user_id, not both. Notifications cover raids that appear in chat. Payload includes from/to ID, login, name and viewers. There is no documented ordering or latency guarantee relative to redirect. Use notification metadata time, not a fabricated event timestamp.
- [WebSocket handling](https://dev.twitch.tv/docs/eventsub/handling-websocket-events/): reuse welcome session ID; current documented limits are three enabled connections per user/client tuple, 300 subscriptions per connection, and shared total cost 10. Ordinary disconnections can lose events; notification IDs may repeat. This is best effort, not a guaranteed redirect follower.
- [Subscription management](https://dev.twitch.tv/docs/eventsub/manage-subscriptions/): WebSocket transport uses a user access token. Inference: this app's existing normal user token is sufficient because channel.raid requires no additional scope; no OAuth changes are needed. Public events for broadcasters who have not authorized the application can consume cost; authorized broadcasters can cost zero. Do not infer free viewer WebSocket subscriptions from the webhook example's cost: 0.
- [Create subscription](https://dev.twitch.tv/docs/api/reference/#create-eventsub-subscription): use existing authenticated Helix POST and existing WebSocket session. Bound watched unique open source IDs at ten, conservatively allowing one cost unit per source; existing subscriptions share the budget and may leave fewer available slots. Actual API acceptance/cost is authoritative.
- Source inspection: EventSub Controller already owns normal user sessions and RAII subscription references. Connection receives raw notifications before the bundled library's typed dispatch. channel.raid lacks a bundled typed handler; the raw bridge can handle it, while the library may log a nonfatal unsupported-handler warning and continue reading. No dependency changes are necessary.
- Source inspection: PubSub raid_update_v2 is countdown/status information, not a reliable completed-raid trigger. channel.moderate raid handling needs privileged moderator authorization. Incoming IRC USERNOTICE is target-side only. Neither substitutes for outgoing public channel.raid.
- Watch only real Twitch source IDs present in open splits, including aggregate children, regardless of current live flag (avoids stale live metadata). Maintain a bounded enabled-only scan, release handles on close, disable or account change, and do no extra scans/timers/subscriptions while off. Shared service handles one outgoing subscription per unique source.
- Reuse normal channel registry and notebook APIs; guard GUI delivery, source/account/setting changes, blocked targets, drafts and application focus. Append new tabs at the source window's end; selecting an existing tab must respect the same switch guard.
- Controller source has an in-flight create/unsubscribe race: a late successful POST can arrive after its last handle was removed. Feature implementation must clean up that returned subscription as well.

Live EventSub delivery, authorization, subscription cost/availability and redirect ordering: **NOT RUN**. These conclusions come from official documentation and source inspection; no Twitch network action was performed. Nothing has been compiled.

## Implementation decisions

### Commit A: a0e4f0bde

- Deleted automatic contrast/background shading helpers. Both highlighted shapes use the unchanged chosen QColor for pen/fill (no inset outline). Stock circle geometry, shape selection, star factor and radius clamping remain. Alpha is retained in JSON via HexArgb; the existing swatch dialog exposes alpha. Opaque interior pixels equal the chosen colour; translucent paint retains its chosen alpha and naturally composites with the destination.
- Shared pure pin geometry mirrors the previous right margin on the left, with true vertical centring independent of tab location/selection. TAB_PIN_SCALE = 0.85 scales the original glyph and stroke together. The original 16-scale-unit hit target remains usable. The title's left edge advances past it; the old right reservation is removed. The unchanged normalTabWidthForHeight still reserves pin-or-close width exactly once.
- getXRect now supplies that same left rect to paint, tooltip and double-click paths. Left press still selects (hasXButton is false for pins); pinned release still prevents middle/X close; drag starts from the existing whole-tab logic; right-click still opens the existing Pin/Unpin menu. The pin is still display-only and does not become a new unpin button.
- Star collision bounds use the transformed left pin's complete round stroke; title ink and visible close glyph remain obstacles. Exhaustive fixtures retain compact/normal, all four locations, selection and 1/1.25/1.5/2 scales, including zero-radius suppression.
- Redeem options read the live setting. Pinning changes immediately clear cached IDs and rebuild current list/detail on the GUI thread; a foreign-thread change queues against the dialog context. orderRedeems clears/repopulates IDs and passes enablePinning to displayOrder. Both card/preview paint paths also read the current setting through redeemPinVisible; a stale detail click cannot change pins while disabled. Saved data is never cleared. Existing filters, pin-only channel identity and manual redemption methods remain unchanged.

### Commit B: outgoing raid tabs

- Two registered settings only: /satorino/raids/openTab defaults false, /satorino/raids/switchTab defaults true. The dependent checkbox is greyed out while opening is off. Tooltip explains the ten-source ceiling/shared budget. A stale highlight tooltip was corrected to describe exact colour.
- WindowManager owns one QObject service. While enabled and logged in, a one-second GUI scan watches at most ten unique open real Twitch IDs, including Twitch children of aggregates and hidden tabs. Existing watch slots are stable; new slots initially follow sorted IDs. Channels beyond the ceiling wait for a slot. Live status is not a gate. Sources that close release handles within one scan; event handling independently rechecks that the source remains open.
- OFF stops the timer, releases all handles and disconnects account observers. There are no recurring scans, network requests or new subscriptions while dormant; already in-flight requests may finish and require cleanup. No follow-list scan or extra connection implementation was added.
- Uses existing normal-account EventSub sessions and Helix POST with the same credential snapshot, never a new scope. Account-about-to-change releases old handles before Helix changes auth. Raid-only request identity includes owner ID; other topics retain their previous equality/hash behavior. Generation checks prevent stale queued work, retry/error callbacks and late success responses from attaching to a replaced lifetime. A rejected successful POST gets one redacted DELETE using its original in-memory credentials, even after an account change. Revoked credentials/server failure can prevent deletion; the generic warning contains no token or response body. Socket disconnection/server cleanup remains authoritative.
- Connection owner/subscription state is synchronized between WebSocket reads and Helix callbacks. Reconnect snapshots are copied under that lock and passed to Controller after releasing it (no reversed lock ordering). The raw notification bridge checks the setting before extra parsing and posts against the application QObject. GUI entry checks shutdown, current owner/account, setting and source membership again. QPointers guard window/page/split use.
- Pure parser accepts only outgoing channel.raid v1 conditions matching the source. It uses notification metadata UTC time and normalizes target login; invalid IDs/logins, self raids, anonymous/non-Twitch/closed sources and blocked targets are ignored. Notifications before the enable/account epoch, older than three minutes or more than 30 seconds in the future are ignored.
- Pure policy dedupes source/target pairs for three minutes (bounded to 128 recent pairs). New tabs are limited to one per 15 seconds and five per ten minutes. Existing-target selection does not consume a new-tab slot. Caps/dedupe use a monotonic clock and survive option/account changes; no timer runs for expiry pruning.
- Lookup covers every window/split, including aggregate children. Existing targets are reused; selecting an aggregate target also chooses its active child when switching is permitted. New targets use TwitchIrcServer::getOrAddChannel and normal addPage/appendNewSplit/setChannel APIs, so normal IRC joining applies. Tabs append at the source window's end (the explicitly allowed fallback, preserving pinned-group ordering).
- Switching requires both options, an active main window and an empty selected split input. Selection is rechecked just before use. Background creation restores the prior focus widget because appendNewSplit has an internal focus request. Existing targets get no selection/activation when switching is forbidden. No call sends chat, runs /raid or redeems points.

## Files

| Area | Files / purpose |
|---|---|
| Indicator / pin geometry | src/util/SatorinoIndicator.{hpp,cpp}; src/widgets/helper/NotebookTab.cpp and NotebookTabSatorino.cpp |
| Colour persistence / picker | src/util/SatorinoGames.cpp; src/widgets/settingspages/SatorinoGamePicker.cpp; SatorinoLivePage.cpp tooltip |
| Redeem dormancy | src/util/RedeemPin.{hpp,cpp}; src/widgets/dialogs/ChannelPointsDialog.cpp and ChannelPointsDialogSatorino.cpp |
| Pure raid policy / parser (new) | src/util/SatorinoRaid.{hpp,cpp} |
| GUI raid service (new) | src/controllers/raids/SatorinoRaidController.{hpp,cpp} |
| EventSub hooks / cleanup | src/providers/twitch/eventsub/{Connection,Controller,SubscriptionRequest}.{hpp,cpp} |
| Ownership / settings | src/singletons/WindowManager.{hpp,cpp}; Settings.hpp; src/widgets/settingspages/SatorinoPage.cpp |
| Tests | tests/src/SatorinoRaid.cpp (new, ten cases including a 512-context decision table; 14 net test additions across this batch); Settings.cpp; SatorinoIndicator.cpp; SatorinoGames.cpp; RedeemPin.cpp; RedeemPinFix.cpp |
| Registration / notes | src/CMakeLists.txt marked block; tests/CMakeLists.txt; docs/SATORINO.md; REPORT-F8.md |

## Windows-oriented source review

- All externally called notebook/split/registry/service methods were checked in their public declarations. Notebook::items() is protected, so scanning uses public getPageCount/getPageAt; that count includes hidden tabs. NotebookTab::update visibility/mock behavior is untouched.
- Direct includes were checked for new Qt/standard dependencies: QPointer, QTimer, QMetaObject, QDateTime, QJsonObject/Document/Value, QChar, QString, QtGlobal, QUrl/Query, mutex, memory, deque, map, set, functional, cstddef, algorithm, optional and utility. New project include paths resolve. No invented IndirectChannel header is used: its public definition is in common/Channel.hpp.
- Setting callbacks added/edited here have explicit bool or zero-argument signatures, not generic auto parameters that can confuse pajlada overload resolution. Generic visitors are ordinary std::function conversions with fixed argument types.
- GUI changes are context-guarded; queued functors do not retain raw window pointers. Account observers are disconnected while off and scoped to service lifetime. HTTP callbacks retain request generations/credential values, never GUI widgets. Lock order is Controller -> Connection; raw callbacks release the Connection lock before posting/calling Controller.
- Integer geometry uses explicit truncation where stock calculations do; no new narrowing braced initializers or out-of-order designated initializers were introduced. qint64 is used for timestamps/caps; std::size_t indexes spans; Qt size conversions do not appear in narrowing braces. The new code uses C++20/23-supported erase_if and ordinary lambdas.
- No workflow, sync script, badge, dependency/submodule, OAuth-scope or account-manager source changes. Existing NetworkRequest/Helix interfaces and other EventSub request identities are unchanged. Private Controller return/signature extensions retain the existing non-raid paths.
- No further compile error found by source review. This is not a compiler result.

## Executed vs NOT RUN

| Check | Result / evidence |
|---|---|
| Read project conventions; fetch origin; base/ancestry/clean-tree check | PASS; BATCH7_HEAD above equals origin/satorino |
| Official documentation / existing EventSub/PubSub/IRC and public API source review | Executed; B0 conclusions and limitations above |
| clang-format 19.1.7 on changed ranges/new files, followed by --check | PASS; 31 C++ files, zero formatting differences |
| Full-range git diff --check | PASS |
| LF bytes, source/test CMake registration, new local include paths, forbidden-path/action audit | PASS; batch8/source-audit.txt |
| Clean verification clone at BATCH7_HEAD; git am --3way of commit A | PASS; tree 6239aeee086a8c1b8e93b4dedf834dceb98b0ac7 equals original commit A |
| Final two-patch export/application and final tree equality / clean status | Performed immediately after the final commit; exact result in batch8/verification.txt and the exported final report, generated after commit |
| C++ compilation (Windows Qt 6.12 / clang-cl / C++23; Linux GCC) | **NOT RUN**; no cl/clang-cl/clang++/g++/cmake/ninja found on PATH; neither C:/Qt nor C:/Program Files/Microsoft Visual Studio exists |
| All C++ gtests, including offscreen pixels / geometry / redeem order / raid policy and parser | **NOT RUN**; nothing was compiled |
| Windows UI, live Twitch authorization/cost/delivery/reconnect and IRC target join | **NOT RUN**; no live network action performed |

## Windows checklist (all NOT RUN)

- [ ] Build Qt 6.12 / clang-cl / C++23 and run registered gtests, including SatorinoIndicator, SatorinoGames, RedeemPin/Fix, SatorinoRaid and Settings raid defaults.
- [ ] Dark/light themes, selected/unselected/hovered tabs: game star AND circle use the exact chosen opaque colour; alpha persists across restart and blends normally. No outline; circle size remains stock.
- [ ] Pin is 15% smaller on the left and vertically centred for top/left/right/bottom, normal/compact styles and 100/125/150/200% DPI. Short/long titles stay clear; width/size hints unchanged; star clamps without overlapping title/pin/close.
- [ ] Pin tooltip and double-click area are on the left. Left click still selects; right-click Pin/Unpin, middle-close protection and drag behavior match batch 7. Old right position has no pin tooltip/hit behavior.
- [ ] With stored redeem pins and an already-open list/detail, turn pinning off: normal filtered order, no yellow stars or pin button. Re-enable: pins/order/button return. Persisted pins survive restart. Manual redemption flow/channel identity unchanged.
- [ ] Raids default off: dependent switch disabled, no raid subscriptions or running scan timer. Turning off while a POST is pending releases handles and deletes late successes; no stale tab is opened. Test account switch/logout and closing windows/sources during pending work.
- [ ] Enable logged in; a watched source raids out: target tab appends to source window (end fallback), normal chat join, no automatically sent text. Test real Twitch sources within aggregates; Kick/non-Twitch never watched. At most ten unique source watches, fewer if shared budget is exhausted; close a source and confirm slot cleanup/reuse.
- [ ] Switch enabled + main active + empty current input selects the new target; switch off, external app active or a current draft creates it without selection/focus steal. Existing target in this or another window/aggregate is reused under the same guard; no duplicate tab. Repeat after dedupe expiry to select existing.
- [ ] Duplicate source/target within three minutes is ignored; rapid different targets create at most one tab/15 seconds and five/10 minutes. Expiry permits later opens. Toggle/account changes do not reset caps. Blocked/self/invalid/stale notifications do nothing.
- [ ] Exercise reconnect, unavailable auth/cost slots and quitting. Missing bundled typed handler may log a nonfatal warning; confirm reading continues. There is no promised redirect ordering and no recovery of events lost during an ordinary disconnect. Failed subscriptions follow existing Controller error/retry behavior; closing/reopening the source or toggling the feature starts a fresh attempt.

Nothing was compiled. No push was performed. Auto respond remains absent.
