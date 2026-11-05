# Stage F batch 9

BATCH8_HEAD: 5a0aff4cca3db87ab7c97fdac6d9b5cd193f0886
Base tree: 067c850b49b0eba8d1e284363fd3b8c8b8d90e64
Commit A: b342c3e95e515d89c86cbd62dfb4f42ec30e1e09

Setup: fetched origin. origin/satorino is still 0b329736eef878f2e356d3d671b93b537f63e55b (Batch 7) and does NOT contain Batch 8. origin/satorino..stage-f initially contained the two Batch 8 commits. Continued on the existing clean stage-f branch as requested, without reset/rebase/re-cloning it. No push.

## A: visuals

The tiny star is explained by source: the old painter.boundingRect supplied a whole title line box, and indicatorStarRadius could reduce the radius to zero for any obstacle. The tighter pinned title reservation made that pressure more visible. No live UI measurement is claimed.

- INDICATOR_STAR_FACTOR is now 1.7; INDICATOR_STAR_FLOOR is the Batch 7 value 1.45. Obstacle pressure cannot reduce below the floor. The tab's own edge limit remains hard, including physical-pixel clearance, so only edges can force a smaller radius. The stock dot anchor, inner ratio 0.5, exact QColor/alpha and circle paint/coverage path are unchanged.
- Title obstacles use tightBoundingRect placed at the real horizontal alignment/font baseline, clipped to the pinned title area. Only an approach from the left of the corner is considered; title padding no longer dictates an arbitrarily tiny star.
- TAB_PIN_SCALE is 0.7. The original stroked glyph bounds plus two scale units TOTAL padding (one per side) replace the old 16-unit box. It sits at a four-unit left margin; ink, not the asymmetric path origin, is vertically centred. The title follows the box by about two units.
- Pinned size hints reserve that small box once and preserve the full measured title width (no old close padding/150-unit title cap). Ordinary non-pinned size hints are unchanged. With a representative 55-unit title at scale 1, the new width is about 73 normal / 72 compact versus the former 87 / 76; exact font/DPI rounding varies.
- getXRect supplies paint/tooltip/double-click hit geometry. Existing left-select, right-click Pin/Unpin, pinned middle-close protection and whole-tab drag logic remain. The star obstacle uses the same transformed pin stroke.
- Final review found that preserving the floor can touch the compact side-tab close glyph at its stock position. The final commit includes a small visual follow-up: only the X ink moves down within its existing hit/hover box when a highlighted star is shown. Shared geometry drives its paint and obstacle; the hit box, mouse behavior, width and circle/non-highlighted paths stay unchanged. History was left intact, with exactly two Batch 9 commits.
- Updated pure scale/location/style/selection fixtures assert floor versus edge limits, no tab clipping, no control intersection, tight pin bounds/width and full-title room. Existing exact-colour/alpha and stock-circle coverage tests remain.

## B1: end-to-end source diagnosis

The most likely APP-SIDE break is subscription establishment/availability, rather than the own-channel/offline policy. Batch 8 never checked whether the startup controller was real or whether Helix accepted the request; an empty dummy handle or a permanently failed request could look like a watch. This is an inference from the silent failure paths, NOT proof of the user's particular run. New diagnostics distinguish these cases. A chat-visible Twitch event not being emitted for the offline test is another unproven possibility.

| Link | Evidence and assessment |
|---|---|
| Setting -> configure | Settings.hpp registers /satorino/raids/openTab false and switchTab true; SatorinoPage binds the same settings. RaidController connects the option to a context-guarded configure call. No alternate key or stale snapshot found. |
| Login/token/owner/timer | RaidController::accountChanged requires a normal logged-in account and token, sets ownerId, then scans/starts a one-second timer only for an actual enabled Controller. Status distinguishes not logged in / missing token / startup disabled. stop releases watches and stops the timer. |
| Open source room ID | IrcMessageHandler.cpp:532 reads ROOMSTATE room-id and calls setRoomId without checking live state; TwitchChannel::setRoomId emits roomIdAssigned. This proves there is no local live gate, not that a ROOMSTATE arrived in the user's run. sync revisits all real Twitch splits/aggregate children and picks up later IDs. New own-channel fallback uses the already-known logged-in account ID if the joined room ID has not arrived; other missing IDs are shown as waiting. |
| Actual startup controller | Application.cpp:156 makeEventSubController is called by Application's constructor at line 197, once. enableExperimentalEventSub is registered at Settings.hpp:1029 with default TRUE, not false. Saved false still selects DummyController, whose subscribe returns an empty handle. General -> Advanced exposes Enable experimental Twitch EventSub support (requires restart), GeneralPage.cpp:1672. Changing it cannot replace the current controller. New status reads actual runtime availability plus current setting, so it says enable-and-restart or restart-needed accurately. |
| Request routing/auth mode | Controller accepts arbitrary request types; no raid whitelist blocks POST creation. getViableConnection/canHandleSubscription check owner ID and normal-versus-alternate auth. Raid requests leave helixOAuthToken empty, so they use/reuse NORMAL sessions, not the alternate Moltorino connection. The normal account credentials are now privately captured on the GUI thread before Asio posting; no mutable account reads remain in the raid worker path. |
| Connection/Helix body | Controller::subscribe waits for a viable welcome session ID and calls Helix::createEventSubSubscription. Helix.cpp:3617 builds type=channel.raid, version=1, condition={from_broadcaster_user_id: sourceId}, transport={method:websocket, session_id: welcomeId}. B9 does not change this request body or OAuth scopes. Subscription status is read from the locked Controller map, not inferred from handle ownership. |
| Helix errors | Existing mapping is HTTP 400 -> BadRequest or NoSession for the session-does-not-exist message; 401 Unauthorized; 403 Forbidden; 409 Conflict; 429 Ratelimited; 500/other/network failures Forwarded. Raid-only error forwarding now preserves exact HTTP status before redaction; non-raid callbacks/public signatures stay unchanged. UI shows the short message and backoff. |
| Raw callback/payload | lib/twitch-eventsub-ws/src/session.cpp:525 calls onNotification(metadata, *payloadV); line 589 calls listener BEFORE typed lookup. jv is the PAYLOAD with subscription/event, not the full envelope. Metadata optional string comparisons are standard optional/string comparisons. parseRaidEvent accepts this real payload and additionally validates/unpacks full-message samples. New offline test exercises this actual library entry and nanosecond metadata parsing; C++ execution NOT RUN. |
| GUI bridge/owner | Connection::onNotification copies the payload without logging it and posts using the application QObject context; WindowManager forwards on the GUI thread. Original owner checks remain. A first event can precede its POST response, before Connection has an owner; new pending-owner lookup requires the actual session/source and a live request, never simply assumes the current account. Shutdown, source membership and account changes remain guarded. |
| UTC/epoch/age | Helpers.cpp:434 casts system_clock time to milliseconds and constructs QDateTime from epoch milliseconds. toMSecsSinceEpoch returns the same instant even if the displayed QDateTime is local time, so no timezone-offset bug was found. B8 compared SERVER event time against LOCAL enable time, which can falsely reject an early legitimate event under small clock skew. B9 compares LOCAL arrival time against enable epoch; metadata UTC still enforces age <=3 minutes and future <=30 seconds, with explicit stale/check-clock reasons. Truly large system-clock errors can still reject events; no claim otherwise. |
| Receive -> policy -> action | receive logs every parsed decision or early rejection. It rechecks logged-in/token/current owner, active watch request, source still open, blocked target, source/target validity, self raid, age/epoch, dedupe and caps. There is NO condition rejecting sourceId==ownerId and no isLive requirement. Existing targets in all windows/aggregates are reused; otherwise normal registry/addPage/split APIs join the target. Switching needs the option, active main window and empty current input. Status distinguishes opened/reused/switched/not-switched/ignored and why. |

### Official Twitch checks (read, not live-tested)

[Channel raid documentation](https://dev.twitch.tv/docs/eventsub/eventsub-subscription-types/#channelraid) specifies channel.raid v1, no event-specific authorization, either from or to condition (not both), and source/target IDs/logins/names plus viewers. It only notifies for raids that appear in chat. There is no documented redirect-order guarantee. An offline raid test completing in Twitch's UI is not proof that this chat-visible event was emitted; that possible explanation is an inference, not a diagnosed fact.

[Subscription management](https://dev.twitch.tv/docs/eventsub/manage-subscriptions/) requires a user token for WebSockets and describes zero cost for authorized broadcasters versus potential cost for public subscriptions to others. No special raid scope or broadcaster-ownership restriction is listed. Reusing the existing token for one's own or another open channel follows that documented authorization model; actual acceptance here is NOT RUN. Limits remain shared: three enabled connections, 300 enabled subscriptions/connection, total cost ten. The ten-source local watch ceiling remains; existing app subscriptions can reduce available budget.

[Create subscription API](https://dev.twitch.tv/docs/api/reference/#create-eventsub-subscription) documents WebSocket user-token/session transport and error responses. 401 can mean invalid/mismatched client credentials, 403 missing required authorization, and limit/conflict errors must be observed rather than assumed away. New UI diagnostics retain the actual returned status/message; no new token flow or scope was introduced.

[WebSocket handling](https://dev.twitch.tv/docs/eventsub/handling-websocket-events/) documents reconnect preservation, revocations and the possibility of lost notifications on ordinary disconnection. The new code retries/reports these states; it cannot recover events Twitch did not deliver.

## B2: library error path

Verified source: Session::onNotification called the raw listener, then returned NoMessageHandler for channel.raid because the generated registry lacks it. onRead passed that error to fail(). fail() logs a warning and calls listener.onClose ONLY IF ws.is_open() is already false. An otherwise-open socket continues reading; the missing handler was therefore NOT demonstrated to close/reconnect it.

Fixed regardless, as requested: two small marked hooks in handwritten session.cpp (ten added lines), no generated source or handler table edits. Raw-consumed channel.raid v1 returns success before typed lookup. Raid revocations also reach that raw listener, set the corresponding live request to Failed and become eligible for retry. Other event types keep the existing dispatch path. The offline dispatch regression expects no error for raid/revocation, verifies payload-only shape and parsed UTC timestamp, then handles a keepalive with the same listener. No socket is opened by that test; it is NOT RUN here.

## B3/B4: visible diagnostics and recovery

- Exactly one grey, plain-text status label under the existing two raid checkboxes; no buttons. Its one-second refresh timer starts in showEvent and stops in hideEvent. The brief line shows gates, up to three watch names/states, first failure/retry delay, and last event/action; tooltip lists every bounded watch. It explicitly distinguishes no event received from an event ignored by a guard. No payload/token is displayed.
- Matching qCInfo/qCWarning transitions use chatterinoTwitchEventSub. Retry countdown alone does not produce per-second logs. Each received raid has an action or ignore reason. Raw raid debug-payload logging was removed from this path; HTTP error messages redact the captured token, bearer/OAuth material, URLs and object payloads and are bounded to 160 characters.
- Failed or lost handles are released/retried after 5/10/20/40/60-second backoff while enabled. A pending attempt times out locally after 90 seconds and becomes retryable. sync also notices changed token/client/owner and restarts the watch set, picks up late room IDs and releases closed sources. Actual subscribed state resets failure backoff. Off disconnects account observers, releases watches and stops scanning; page-only text refresh is still allowed by B3.
- A Twitch-directed reconnect previously left the raid subscription map pointing weakly at the old Session. The replacement Session is now returned from the private creator and assigned to raid subscriptions so status/unsubscribe follow the active connection; other topic behavior is unchanged. Actual revocations now clear the ID/connection and mark failure. Refcount/state checks avoid interfering with an unsubscribe already in flight.
- Existing generation checks and original-credential late-success DELETE cleanup remain. Credentials are private memory only, never serialized into the request identity/report/log. Controller/Connection locks keep the existing Controller -> Connection order; raw callbacks release their lock before calling/posting to Controller.

### Local test command

Use /satorino-testraid <targetLogin> [targetId] in the current real Twitch channel, while raid opening is on and logged in. If the target exists in the local registry with its room ID, the ID argument is unnecessary. To test opening a genuinely new tab without a metadata lookup, supply the correct numeric target ID. An ID conflicting with a known local channel is rejected; no ID is fabricated and no Helix lookup is made.

The command always returns empty chat text, including off/invalid cases, so it is consumed by the existing command infrastructure. TwitchChannel::sendMessage returns on the resulting empty prepared text. Only local system messages show results. Simulation is queued against WindowManager until the submitted command leaves the input, so that command does not falsely count as a draft; a retained/new draft still prevents switching. It calls the same parseRaidEvent -> policy -> tab action and retains every setting/login/source/account/block/self/age/dedupe/cap/draft/focus guard. Only the live transport-subscription-membership check is skipped. Normal target-tab creation still joins chat through the usual registry; it never sends a message, starts a real raid or spends points. Simulation shares live dedupe/caps; repeated probes can be ignored for three minutes.

## Files and Windows source audit

| Area | Files |
|---|---|
| Visual geometry/paint | src/util/SatorinoIndicator.{hpp,cpp}; src/widgets/helper/NotebookTab.cpp and NotebookTabSatorino.cpp |
| Pure raid state/parser/reasons | src/util/SatorinoRaid.{hpp,cpp} |
| UI/service/ownership | src/controllers/raids/SatorinoRaidController.{hpp,cpp}; src/singletons/WindowManager.{hpp,cpp}; src/widgets/settingspages/SatorinoPage.{hpp,cpp} |
| Backend diagnostics/recovery | src/providers/twitch/eventsub/Controller.{hpp,cpp}; Connection.cpp; src/providers/twitch/api/Helix.cpp |
| Local probe | src/controllers/commands/CommandController.cpp; builtin/chatterino/Debugging.{hpp,cpp} |
| Handwritten library fix | lib/twitch-eventsub-ws/src/session.cpp only |
| Tests/registration/docs | tests/src/SatorinoIndicator.cpp, SatorinoRaid.cpp, new SatorinoRaidDispatch.cpp; tests/CMakeLists.txt; docs/SATORINO.md; REPORT-F9.md |

No new app .cpp/.hpp files, so the existing marked src CMake registrations suffice. New test is registered. Ten net gtests added across the batch.

Access: notebook/split/channel APIs are public; snapshot/receive/simulation APIs are public; protected NotebookTab::update visibility/mock behavior is unchanged. The library test uses public handleMessage (explicitly exposed for testing). Qt and standard dependencies are explicit, including QTimer/QPointer/QJsonObject/Value/QDateTime/QPalette/QFontMetrics, mutex/unordered_map/vector/chrono/algorithm/optional and Boost buffer/error types. New settings callbacks use explicit bool signatures, not generic pajlada callbacks. New braced aggregates use bool/int/qint64/QString members without narrowing; no designated-initializer order changes. GUI callbacks have QObject contexts; asynchronous lambdas retain safe values/channel shared pointers and recheck state. No further compile error found by inspection; this is NOT a compiler result.

## Executed vs NOT RUN

| Check | Result |
|---|---|
| Fetch/branch/base/initial clean-tree check | PASS; origin/satorino behind Batch 8, continued as instructed |
| Read project conventions; full chain/library/error/auth/UTC/mouse source review | Executed; conclusions above |
| Official Twitch docs | Read; live auth/cost/delivery NOT RUN |
| clang-format 19.1.7 changed ranges/new test, then --check | PASS; 23 C++ files, zero differences |
| LF/forbidden-path/generated-source/action/CMake registration audit | PASS; source-audit.txt |
| Full-range git diff --check | PASS |
| Clean clone at BATCH8_HEAD, git am --3way of commit A | PASS; tree 818be17df9b650214499eae1f2bb3df397c62320 matches original A |
| Complete two-patch application, final tree equality and clean statuses | Exact post-commit result written to batch9/verification.txt and exported REPORT-F9.md |
| Compile Qt 6.12 / clang-cl / C++23, or Linux GCC | **NOT RUN**; cl/clang-cl/clang++/g++/cmake/ninja unavailable on PATH. Nothing compiled |
| All C++ gtests (pure/pixel/geometry/status/parser/offline library dispatch) | **NOT RUN** |
| Windows UI, local-command execution, real Twitch raid/token/cost/reconnect/revocation | **NOT RUN** |

## Windows checklist (all NOT RUN)

- [ ] Build Windows Qt 6.12 / clang-cl / C++23, run registered tests including SatorinoIndicator, SatorinoRaid and SatorinoRaidDispatch.
- [ ] Star is visibly larger than Batch 7 when space allows, at least its floor unless tab edges constrain it; test pinned/unpinned normal/compact, all four locations, 100/125/150/200% DPI. No tab clipping or pin/X contact; circle and exact colour/alpha remain unchanged.
- [ ] Pin is smaller, centred, with a tight left tooltip/hit rectangle; pinned tabs narrower with full title room. Left select, right-click Pin/Unpin, double-click, drag and middle-click protection unchanged. Non-pinned widths unchanged; highlighted close glyph remains inside its usual hit box.
- [ ] Satorino raid status off/on, logged out/in, token missing, no channels, delayed room ID, pending/subscribed/failed watches. Hover lists all watches. General -> Advanced EventSub off at startup says enable-and-restart; on after saved-off says restart-needed; restarting selects the real controller. Default is on, not proof of a particular saved value.
- [ ] Own channel open while offline becomes a real watched source, or shows the exact missing stage. Confirm a subscribed watch versus merely pending/subscribing. A real chat-visible raid to alt opens/reuses its tab or shows the event ignore reason. If watching but no event appears, compare whether Twitch emitted a target-chat raid notice; do not infer delivery from redirect alone.
- [ ] /satorino-testraid alt with a locally known ID reuses its tab; supplying a correct ID for a new target opens one. Every result is a local system message/status update. Off/anonymous/Kick/invalid/self/blocked cases do nothing; no chat or real raid is sent. Dedupe/caps apply to probes too.
- [ ] Switch on/off, main inactive/external app, current draft/keepInput, target already in another window/aggregate: no duplicate or focus steal. Close source/window during a queued probe/real event; account/setting changes cannot act on stale context.
- [ ] Failed HTTP 400/401/403/409/429/5xx/network errors expose short reason and retry timing. Refresh token, delay room ID, disconnect/reconnect, revoke a raid subscription, then verify recovery/backoff and ten-watch bound. Disable during pending POST: references/retries stop and late successes get cleanup.
- [ ] Raid notification/revocation returns success through the raw library path; no NoMessageHandler/fail/reconnect caused by a consumed raid. Other EventSub features continue. UI/logs contain no token/raw raid payload; hidden settings page stops its refresh timer.

Nothing compiled. No push, workflow/sync/badge/OAuth-scope/dependency/generated-code changes. No automatic chat or point actions; auto respond remains absent.
