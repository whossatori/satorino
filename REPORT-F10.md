# Stage F batch 10

BATCH9_HEAD: ef2e68196d5eae3e19fed3891223b459e45ee804
Base tree: cb2c3b161eaf1150d8a272b73159d40f689bb67c
Pre-batch-8 restoration base: 0b329736eef878f2e356d3d671b93b537f63e55b

Fetched origin. origin/satorino remains at the restoration base (Batch 7), four commits behind stage-f at setup, with zero divergent remote commits. Initial stage-f tree was clean. Continued in the existing clone/branch; no reset, rebase, history rewrite, re-cloning of the working repository or push. One coherent new commit contains the feed replacement and removal of the old path.

## Diagnosis confirmed and corrected

The user's source identification was correct. My Batch 8 dismissal of this feed as countdown-only was too narrow: I stopped at the client's current type filter instead of extending the source already driving the working raid bar.

| Link | Source evidence |
|---|---|
| Public raid topic | PubSub::listenToRaids in PubSubManager.cpp constructs raid.<channelId> and subscribes TopicData with the topic only, no auth token. Authenticated topic helpers elsewhere are separate. |
| Existing transport | PubSubClient's raid branch extracted raid or data.raid, added type/topic and emitted PubSub::raid.updated. Its only accepted type was raid_update_v2; other types were explicitly rejected. |
| Existing bar | Every TwitchChannel connects to raid.updated, filters its own room ID, marshals to GUI and calls handleRaidUpdate. That parses raid ID, source/target ID, target login/display name/image, viewer count, force duration and creation/receipt time, then setActiveRaid -> raidChanged. SplitInput's bar derives remaining time from creation-or-first-receipt plus force duration. |
| Existing listen gate | TwitchChannel::refreshPubSub previously called listenToRaids only with showRaidStatusAboveInput true. Its setting is exposed in MoltorinoPage. No live-state check gates raid LISTEN here; empty room IDs wait for the existing roomIdChanged refresh path. |
| Why Batch 9 missed channels | Its independent EventSub set stopped at ten sorted/stable IDs. The user's watched 10/10 status did not establish that their own channel was included. That selection/cap and the entire separate transport are removed, not increased. |

The shared feed has no EventSub scope, cost budget or ten-channel watch cap. Existing PubSub client/topic pooling and transport limits still apply; this does not claim an unlimited server capacity. Only open real Twitch split/aggregate sources can trigger tab actions.

A [firsthand historical raid capture](https://gist.github.com/JayJay1989/2f3049ac14ea293fe7d866b2125beab9) shows raid_go_v2 with the same raid object fields (ID/source/target/login/name/viewer count) and notes duplicate delivery. Its update shape matches the existing bar parser. This is primary schema evidence from 2020, not proof of current go/cancel delivery in the user's running client. Current live type/payload delivery is **NOT RUN**. The expected cancel type is accepted defensively even without a full raid object; local existing cancellation/state-clear paths also stop fallback.

## Implementation

### Shared feed and live settings

- PubSubClient now accepts raid_update_v2, raid_go_v2 and raid_cancel_v2 via the pure normaliseRaidFeed adapter. Update data is preserved; go/cancel can carry raid, data.raid, or a raid_id/raidId in the root/data object. The transport topic is authoritative. Unsupported types and malformed updates remain rejected.
- RaidController has one central raid.updated connection and no per-channel watcher set or polling timer. The cross-thread callback retains no controller/window pointer: it copies payload/arrival time and posts using the application QObject context. WindowManager routes to the existing owned GUI service.
- TwitchChannel's condition is showRaidStatusAboveInput OR satorinoRaidsOpenTab through the tested helper. Explicit bool callbacks on both options refresh already-created channels immediately; room ID arrival uses the existing refreshPubSub path. New rooms use normal construction/room assignment. No restart is needed, and EventSub's startup setting is irrelevant.
- Both options off creates no new raid LISTEN calls and stops the new consumer/deadline timer. The original PubSub manager's topic lifetime is retained: like the old bar, already-requested topics may remain in its pool until normal connection/application cleanup. No new unlisten/reconnect policy was invented. The tooltip/count describes requests registered with that shared manager, not per-topic server acknowledgement or guaranteed delivery.
- Bar updates still follow the original parser/state path. Go is not treated as a new countdown, so it cannot reset the bar. Cancel clears only a matching active raid (or its current raid when ID is absent). Other topics and automatic chat/redemption behavior are unchanged.

### Trigger, cancellation and fallback

Primary trigger: raid_go_v2. The pure tracker maps source from raid.<id>, verifies any payload source matches, and retains target ID/login/display name plus non-negative viewer count. It supports the existing snake/camel aliases and numeric/string count/time representations. A partial go can reuse the matching known countdown target; an explicit mismatch disables a stale-target fallback.

Fallback: one cancellable, single-shot deadline from raidCreatedAt + forceRaidNowSeconds, or the FIRST accepted update receipt plus that duration when creation time is absent/unusable, matching the bar's timing model. Repeated updates with no creation time do not slide the deadline. Missing/nonpositive duration defaults to the existing bar's 90 seconds; unreasonable durations above one hour disable fallback and wait for go. There is up to one second of grace at the earliest boundary to let queued go/cancel arrive. This is a fallback inference from countdown expiry, not proof of a redirect; status explicitly says countdown end when it is used.

- Updates alone never open a tab before the deadline. Go or expiry fires once; the later duplicate/other trigger is ignored. Cancel suppresses both, and short cancellation tombstones reject reordered old go/update even after a newer raid begins.
- Existing clearActiveRaid cancellation/unraid/IRC paths forward a local cancel into this same consumer, so a cancellation does not depend on receiving raid_cancel_v2. Early setActiveRaid(null) uses that cancellation route; the bar's ordinary clear AT expiry does not suppress expiry fallback.
- One existing sendRaidNow SUCCESS callback calls clearActiveRaid(true) so an already-confirmed send is a go, not a cancellation. This changes only post-success state notification. No request to start/send/cancel a real raid was added or invoked by this feature.
- Account/setting changes disconnect the feed and stop/reset pending deadlines. Closed sources are pruned before handling a feed/deadline event. There is no polling loop; the only service timer is a single-shot deadline while a valid countdown exists.
- A cancel arriving after an action already committed cannot undo a tab. Network delay/actual type availability remain live-test limitations; the implementation suppresses cancellations observed before fallback and uses the boundary grace to reduce races.

### Existing guarded tab action retained

The pure policy retains source/target validity, self/blocked/anonymous/non-Twitch/closed-source checks, local enable epoch/age checks, source+target dedupe for three minutes, one new tab per 15 seconds and five per ten minutes. Source and target lookups include all windows, hidden tabs and Twitch children of aggregates. A target already open is reused; a new target appends to the source window through the normal Twitch registry/addPage/appendNewSplit/setChannel path and normal chat joining.

Switching still requires the option, active main window and empty current split input, with immediate recheck before selecting/raising. QPointers and GUI context guards remain. Background creation restores prior focus. No chat message, real raid or channel-point action is initiated. The title/indicator/pin visual files from Batch 9 are unchanged.

### Status line

Same single small grey plain-text label and visible-page-only UI refresh; no new buttons. It reports off, not logged in, shared listening request count for open channels, waiting room ID/feed count and the last source -> target/outcome (countdown, go/tab opened, reused, ignored with reason or cancelled). Tooltip states go / one-shot countdown end and that the source is shared with the raid bar. Counts are not limited to ten. Transition/decision logs use the existing PubSub category, with no tokens or raw payloads. No EventSub retry/HTTP diagnostics remain.

### Test command

/satorino-testraid <targetLogin> [targetId] remains a local command, queued until its submitted input is cleared so it does not count as its own draft. The pure parser validates login/ID. Explicit ID overrides; otherwise a known local channel ID is used. If still missing, exactly ONE Helix users-by-login request is made using existing normal account auth, only because the user ran the command.

The existing getUserByName wrapper conflates empty results and network errors, so the equivalent underlying fetchUsers({}, {login}) call is used to distinguish not-found, error and success without changing Helix. A ten-second local timeout completes the attempt once; late callbacks cannot create a tab. Returned ID/login are validated, and QPointer, weak source, account ID and configuration generation checks protect close/logout/off-on races. No passive metadata lookup, new scope, fabricated ID or retry loop is introduced.

The resolved target becomes a synthetic go payload and uses the same parser/policy/tab action. It does not mutate a live countdown. Dedupe/caps/blocks/drafts/focus/source guards apply equally to probes. Results are local system messages; the handler returns empty text, which existing Twitch send handling discards. Opening chat still uses normal joining, and the explicit users lookup is a read request; the command sends no chat/real-raid/redemption request.

## Old path removed: exact restoration

The following files are byte-identical to 0b329736eef878f2e356d3d671b93b537f63e55b, verified with both git diff and direct blob comparison:

- src/providers/twitch/eventsub/Controller.cpp
- src/providers/twitch/eventsub/Controller.hpp
- src/providers/twitch/eventsub/Connection.cpp
- src/providers/twitch/eventsub/Connection.hpp
- src/providers/twitch/eventsub/SubscriptionRequest.cpp
- src/providers/twitch/eventsub/SubscriptionRequest.hpp
- src/providers/twitch/api/Helix.cpp
- lib/twitch-eventsub-ws/src/session.cpp

All raid-only Controller/Connection APIs, subscription identity/hash edits, normal-token snapshots, late-POST DELETE, retry/state diagnostics and handwritten library raid hooks are gone. Pre-existing fork/upstream behavior, including its ordinary EventSub moderation/raid APIs, remains as it was before Batch 8. This is an exact pre-Batch-8 comparison, not a claim about the latest external upstream HEAD.

Removed obsolete tests/src/SatorinoRaidDispatch.cpp and its CMake entry, the EventSub identity/dummy/status/backoff/envelope-only cases and the unused HTTP-only redaction helper/test. The shared SatorinoRaid suite is registered and now contains 26 pure cases (previous raid suites: 19 cases including dispatch). No new app source files/dependencies were added; existing sources remain in the marked CMake block.

## Files and compile-oriented review

| Area | Files |
|---|---|
| Pure feed/phase/policy/status/lookup | src/util/SatorinoRaid.{hpp,cpp} |
| Central GUI consumer / existing actions | src/controllers/raids/SatorinoRaidController.{hpp,cpp}; src/singletons/WindowManager.{hpp,cpp} |
| Shared source/listening/bar/clear | src/providers/twitch/PubSubClient.cpp; PubSubManager.{hpp,cpp}; TwitchChannel.{hpp,cpp} |
| Local probe and confirmed-clear marker | src/controllers/commands/builtin/chatterino/Debugging.cpp; builtin/twitch/Raid.cpp (post-success marker only) |
| UI | src/widgets/settingspages/SatorinoPage.cpp (tooltip); existing one-line status widget retained |
| Cleanup/tests/notes | Eight restored files above; tests/src/SatorinoRaid.cpp; deleted SatorinoRaidDispatch.cpp; tests/CMakeLists.txt; docs/SATORINO.md; REPORT-F10.md |

Access checked: normal notebook/split APIs and new requested-topic accessor are public; refreshPubSub is called inside TwitchChannel, where its private access is valid. No protected NotebookTab update change. clearActiveRaid's bool has a default, preserving every existing caller; only the already-confirmed success call passes true. State locks are released before invoking GUI/service callbacks. Settings callbacks are explicit bool, with GUI-context guarding in the service; no new generic pajlada signatures.

Direct Qt/standard dependencies include QPointer/QTimer/QMetaObject/QJsonObject/Value/QDateTime/QStringList, optional/map/set/deque/utility/vector/cstdint/limits/cmath/algorithm/functional/memory and unordered_set. Viewer counts/time are qint64, JSON numbers are checked before conversion, timer waits are clamped before explicit int conversion; no new braced narrowing or designated-order changes found. No extra mutable-account reads on a worker. No compile error found by source review, but inspection is not compilation.

## Executed vs NOT RUN

| Check | Result |
|---|---|
| Fetch, branch/base/tree/initial status and origin comparison | PASS; base above, origin four commits behind at setup |
| Project notes/source-chain/go schema investigation | Executed; source findings and historical capture limits above |
| Eight exact restorations: git diff pre-Batch-8 -- files and byte/blob comparison | PASS, zero differences |
| Retired watcher/limit/retry/dispatch cleanup, unchanged visual paths, forbidden action/path checks | PASS; source-audit.txt |
| clang-format 19.1.7 changed ranges, then --check | PASS; 15 implementation C++ files. Eight exact restoration files intentionally excluded to preserve required byte identity |
| LF, full-range git diff --check, source/test registration | PASS |
| Final patch export / git am --3way in clean BATCH9_HEAD clone / exact tree equality and clean statuses | Post-commit results written to batch10/verification.txt and exported REPORT-F10.md |
| C++ compilation (Qt 6.12 / clang-cl / C++23 or GCC) | **NOT RUN**; cl/clang-cl/clang++/g++/cmake/ninja unavailable on PATH. Nothing compiled |
| 26 C++ pure raid tests and existing broader tests | **NOT RUN** |
| Windows UI, current live go/cancel/countdown feed delivery, bar regression, command lookup/timeout and IRC join | **NOT RUN** |

## Windows checklist (all NOT RUN)

- [ ] Build Windows Qt 6.12/clang-cl/C++23; run the registered SatorinoRaid suite and existing related tests. Confirm no removed EventSub API/test references.
- [ ] Enable Open a tab for the channel being raided with the raid bar OFF: own channel and any open live streamer get LISTEN requests and go opens the payload target at go time; no ten-channel selection. Test over ten open channels and aggregate children.
- [ ] If go is missing/partial, valid countdown target opens at countdown end (up to one-second boundary grace), never early. Repeated updates do not slide deadline. Go plus fallback/duplicate go opens only once.
- [ ] Cancel by PubSub cancel, IRC/unraid clear and early active-state clearing: no fallback/open; late go/update after cancellation ignored. Old cancel must not cancel a different newer raid. Confirm send-success clearing is go, and the new feature itself never starts/sends/cancels a real raid.
- [ ] Bar ON keeps its existing countdown/target/viewer display; go does not restart the countdown, cancel clears the matching bar. Flip either option on/off with existing tabs, without restart. Both off retains previous manager/topic behavior and stops the feature's consumer/deadline.
- [ ] Missing room IDs show waiting, then count after normal room assignment. Status shows current source/target and opened/reused/cancelled/ignored reason. Tooltip describes shared raid bar feed and primary/fallback trigger; no EventSub/10-of-10 message or token/raw payload.
- [ ] Existing target in any window/aggregate reused. Switch off, another application active or current draft: no selection/focus steal. Self/blocked/invalid/anonymous/non-Twitch/closed-source cases ignore. Caps/dedupe apply; account/setting/window changes during delivery remain safe.
- [ ] /satorino-testraid <altLogin> with no local ID does one users lookup and opens/reuses its chat. Explicit ID overrides without lookup. Test nonexistent login, error, ten-second timeout/late success, close source/window, logout/account switch and off-on during lookup; no stale tab and only local result messages.
- [ ] EventSub/Helix/library match pre-Batch-8 behavior and other existing EventSub features still work. No raid subscriptions, ten-watch loop, custom retry/late-POST cleanup or generated/dependency/workflow/OAuth/badge changes.

Nothing compiled. No push. Auto respond remains absent.
