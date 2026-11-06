# Stage F batch 11

BATCH10_HEAD: 794030d9ec7fcb12174eabacba8acd3a50f07547
Base tree: a5089b5087ff3cbb8dce7a6453b2712ab9791280

Initial stage-f was clean. Fetch origin completed: origin/satorino contains 794030d9e and exactly matches BATCH10_HEAD (zero remote-only and zero local-only commits at setup). The user's Windows results establish Batch 10 worked in that build; this batch's changes are not yet runtime-tested here. Work continues on the existing branch, without reset or history rewrite, in two requested commits. No push.

## Train research decision (recorded before train implementation/build)

Repo search (src, tests and research, case-insensitive hype/hypetrain/HypeTrain/hype-train): no train transport, parser or active-state provider exists. Only the reserved LiveIndicator::Train priority, TrainColourKind colour extension and its tests exist. TwitchGql contains no HypeTrain query/operation, and the EventSub dispatch has no train subscriptions. Existing IRC USERNOTICE handling has no train start/progress/end state.

PubSubManager::listenToRaids/listenToPolls/listenToPredictions use topic-only TopicData, without a token; PubSubClient explicitly recognizes those topic prefixes. That demonstrates this clone's shared public-topic route, not authorization or current support for every other named topic. A hype prefix could mechanically use the same plumbing, but that alone would not establish correct train/type data.

Evidence checked:

- [Twitch EventSub subscriptions](https://dev.twitch.tv/docs/eventsub/eventsub-subscription-types/#channelhype_trainbegin): begin/progress/end v2 require channel:read:hype_train authorization for the broadcaster condition. An ordinary viewer's existing login does not grant another broadcaster's channel permission. Ruled out; no EventSub subscription/scope is added.
- [Official Get Hype Train Status](https://dev.twitch.tv/docs/api/reference/#get-hype-train-status): requires channel:read:hype_train and the token user to match broadcaster_id. It documents type values regular, golden_kappa, treasure, a separate is_shared_train flag and expiry extended on levels. These are usable model semantics, but not a viewer-accessible producer.
- [Primary twitch-rs PubSub implementation](https://github.com/twitch-rs/twitch_api/blob/main/src/pubsub/hypetrain.rs): contains hype-train-events-v1, an empty scope validator, start/progression/level-up/end/conductor messages and time_to_expire. Its old structs do not establish golden-kappa/treasure/shared type fields (no golden match); several channel fields are optional, with source comments questioning token-dependent availability. This is evidence of an historical potential public feed, not verification of a current typed viewer feed. A v2 topic and its type mapping were not established by the sources inspected. No historical type/payload is guessed into a modern parser.
- [Official IRC concepts/USERNOTICE](https://dev.twitch.tv/docs/chat/irc/#usernotice-tags) and the repo's IrcMessageHandler contain no start/end/type protocol that establishes a running train. Conductor badges and emote availability can outlive a train; a golden Kappa emote is not a running-train/type signal. They are not used as detectors.
- [Twitch Hype Train guide](https://help.twitch.tv/s/article/hype-train-guide?language=en_US): the web UI displays train progress and special variants. The page does not document a viewer API/GraphQL schema or authorization guarantee. Searches for a current primary HypeTrain GQL operation/type contract found no verified operation to reuse; the clone has none. No introspection, browser session/token capture, speculative persisted-query hash, or authenticated live probe was executed.

Decision: a current arbitrary-channel viewer feed with reliable train TYPE has NOT been established. This does not prove Twitch's website has no such private feed. Use the user's explicitly allowed no-data producer route: full pure state/expiry/type/palette/priority/paint/settings/tests, while runtime producer reports no data and creates no network listeners or polling. UI must say train data is unavailable. Missing requirement for a future producer: verified current viewer-authorized topic or GQL operation plus real start/progress/level-up/end IDs, type/shared flags and expiry payloads, followed by live tests on normal and special trains. No new OAuth permissions are requested. The model preserves unknown types as Other; it never infers golden from emotes or assumes absent type means normal.

## Commit A: early-send raid switching

Source trace: SplitInput::handleSendMessage calls execCommand, then postMessageSend clears submitted text unless keepInput is requested. runInGuiThread runs synchronously when already on GUI; the existing sendRaidNow success handler calls clearActiveRaid(true), which can synchronously enter the raid service before the submission stack finishes. PubSub delivery is marshalled separately. There is no runtime trace here proving this was the sole cause of the user's failure. Another confirmed issue is the old main->isActiveWindow() guard, which rejects some in-app popup/secondary-window activation.

The shared tracker completes the raid before queuing one action with QTimer::singleShot(0, controller), the same GUI mechanism used by /satorino-testraid. Input clearing finishes before action/guard evaluation. Setting/account generation and QObject lifetime guards suppress stale queued actions. Go, local success and expiry all use the same completed-state/dedupe path. Main/input state is read at action time and rechecked immediately before selection.

SplitInput tracks its exact currently executing submitted input with QScopedValueRollback, restored on every exit. Only that scoped, byte-identical submission is exempt from the draft guard; arbitrary unsent /raidsend, aliases or other command-looking text remain drafts. After return, keepInput is a real draft. A different/new draft always blocks. ApplicationActive replaces main-window activation; in-app controls/popups/secondary windows do not require Main itself to be active, while another application's foreground blocks switching/activation. The main selected split and an active secondary window's selected split are checked. Prior focus is restored whenever creation does not ultimately switch.

| Early-send path in this repo | Producer / ordering |
|---|---|
| /raid <login>, then /raidsend | /raid starts the existing flow (Helix/GQL), not a go. /raidsend is the registered send command; successful GQL send -> clearActiveRaid(true) local go from saved target, plus possible remote raid_go_v2. Both orderings complete once, then the GUI action runs after input clearing. No request behavior was added/changed. |
| /raid now | NOT a registered send-now variant: startRaid treats now as the target login. No new alias was invented. |
| Raid bar send button | No send-now button exists in this clone's bar: it consists of label/progress widgets. Generic SEND submits input through the same handleSendMessage route. Any other in-app control executing /raidsend uses that registered command. |
| Web/other client sends early | No local success marker; remote raid_go_v2 (if delivered) enters the same queued action. With a valid countdown but no usable go, only the existing deadline fallback can detect completion. Cannot infer an early send from missing transport data. |
| Full countdown | Existing single-shot expiry queues the same action; go/local-success duplicates cannot open again. Cancellation behavior and policy caps remain. |

Pure tests extended for exact submitted text vs genuinely different/unsent command drafts, action-time app foreground checks, early success/go in both orders and no later expiry fire. Source review is not a Windows execution.

## Executed vs NOT RUN

| Check | Result |
|---|---|
| Base/tree/clean status, fetch and origin ancestry | PASS; recorded above |
| Repo/official/primary-source train research before implementation | Executed; conclusion and limits above |
| Commit A clang-format 19.1.7 / diff whitespace / source review | PASS; seven changed C++ files formatted, then zero differences; 29 raid tests registered (NOT RUN) |
| Commit B audits and final patch replay | Pending; updated with final evidence |
| C++ compilation, C++ tests | **NOT RUN**; cl/clang-cl/clang++/g++/cmake/ninja unavailable on PATH. Nothing compiled |
| Windows UI, early real raid, external focus and live train access | **NOT RUN** |

Nothing compiled. No messages, real raids, point spending or push executed. Auto respond remains absent.
