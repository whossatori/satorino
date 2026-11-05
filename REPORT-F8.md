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

Implementation and final verification pending.
