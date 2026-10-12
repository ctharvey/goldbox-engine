/**
 * Draft data API. No QuickJS bindings exist yet.
 * TypeScript will be compiled to JS for a future embedded script host.
 */
export interface JournalPatch {
  readonly target: "journal";
  readonly id: number;
  readonly title?: string;
  readonly body?: string;
}

export interface ModManifest {
  readonly schemaVersion: 1;
  readonly id: string;
  readonly version: string;
  readonly priority?: number;
  readonly requires?: readonly string[];
  readonly patches?: readonly JournalPatch[];
}

export function defineMod<const T extends ModManifest>(mod: T): T {
  return mod;
}
