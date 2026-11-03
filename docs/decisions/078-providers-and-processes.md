# 078: Providers never run processes; one privileged service, through the host

Status: Proposed (refines PS §8 and §9; changes where decision 076's setup lives)

**Context.** File search needs a SYSTEM service (decisions 075, 076), and
its first version let the search code talk to the service's pipe and set it
up itself. Future providers may want work done outside the island too: a
plugin written in Python, a sync that runs on its own, maybe another
feature that needs admin rights. If each provider started, installed and
checked its own processes, every one would repeat the hard parts (restarts,
identity checks, versions, UAC) and any plugin could ask the host to
install something that runs as SYSTEM.

**Decision.**
- **A provider never starts, installs or supervises a process.** Not
  built-ins, not plugins. It asks the host.
- **Helper processes, as the user** (PS §8 Tier 2): declared in the
  provider's manifest; the host starts one when the provider is enabled,
  stops it when it's disabled or the island quits, restarts it after a
  crash or hang with growing delays (1 s, 5 s, 30 s; then it gives up and
  the provider's activities go stale), and talks to it over a per-launch,
  token-checked pipe (PS §9). Not built yet: this fixes the rules for it.
- **At most one privileged service, and it's ours.** No plugin can have one,
  and the host never installs anything a plugin names. A built-in feature
  that needs admin rights becomes a **module** of the same service, not a
  second one: one installer, one UAC prompt in the app's life, one update
  path (decision 076). Methods are named by module: `index.query`,
  `index.status`; `service.update` belongs to the service itself. The exe
  keeps its name (`CupIslandIndex`) until a second module arrives; then it
  becomes "Cup Island Service" in a decision of its own.
- **The host owns the service.** `ServiceHost` (`providers/service_host`,
  a system part of the host like the updater, `system.update`) does what
  only the host may do: finds the service, sets it up once per app version,
  asks it to update, checks that the pipe is the installed service's (its
  process id against the Service Control Manager's), and carries calls.
- **Providers call it through the Host API:**
  `Host::callService(method, paramsJson, done)`. `done` gets the answer's
  JSON, or a problem: not installed, not running, not allowed, no answer.
  A provider sees neither pipes nor process ids, so it stays pure and is
  tested with a fake.
- **Per-provider grants.** The registry lets a provider call a module only
  if it was granted it (`ProviderRegistry::grant`, from its manifest for a
  plugin, PS §9's capabilities, which the user sees and can refuse). The
  built-in search provider is granted `index`; nothing is granted
  `service.update`, which only ServiceHost sends.
- **Versions in one place.** ServiceHost reads the service's protocol
  version (`index.status`) and tells callers "updating" rather than letting
  each provider cope with an older shape (decision 076's rule stands: a
  change of shape bumps `kProtocolVersion`).

**Consequences.** Decision 076's `SearchSetupProvider` becomes ServiceHost,
with the same rules and tests. Its id changes (`system.service`), so its
remembered "asked for this version" moves to a new store key; the setup's
installer writes the new one. The service's methods get module names; no
release has shipped the old ones, so the protocol version stays 1.
