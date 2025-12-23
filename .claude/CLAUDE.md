# yes-ue-mcp - Claude Instructions

**Native C++ MCP Plugin for Unreal Engine 5.6+**

## Project Overview

This plugin implements the Model Context Protocol (MCP) over HTTP, allowing AI assistants (Claude Code, Cursor, Windsurf, etc.) to inspect and analyze Unreal Engine projects through a standardized JSON-RPC API.

## GitHub Repositories

| Remote | Repository URL | Purpose |
|--------|----------------|---------|
| `origin` | https://github.com/softdaddy-o/yes-ue-mcp-private.git | Development (private) |
| `public` | https://github.com/softdaddy-o/yes-ue-mcp.git | Release (public) |

## Workflow Rules

### Issue-Driven Development
- **Every task MUST have a GitHub issue** - Create an issue before starting any task
- **Close the issue** when the task is complete with a commit referencing it (e.g., `Closes #123`)
- Use conventional commit format: `feat:`, `fix:`, `docs:`, `refactor:`

### Git Workflow

**Private Repository (origin)**
- Commit frequently - whenever one issue/task is finished
- Each commit should reference its issue number
- Push to `origin` for development work

```bash
git add -A && git commit -m "feat: add new tool

Closes #123"
git push origin main
```

**Public Repository (public)**
- Push aggregated changes only when a new version is ready
- Use version tags (v1.0.0, v1.1.0, etc.)
- Filter out `.claude/` directory when pushing to public

```bash
# Push to public with .claude/ filtered out
git push public main --force-with-lease
git tag v1.0.0
git push public --tags
```

**Publishing to Public Repo**
When pushing to the public repository, use git filter to exclude private files:
```bash
# Create a filtered branch for public release
git checkout -b release-temp
git filter-branch --tree-filter 'rm -rf .claude' HEAD
git push public release-temp:main --force
git checkout main
git branch -D release-temp
```

## Testing Environment

**Primary Test Project:** Elpis (Action RPG - UE 5.7)
- **Project Path:** `F:\src3\Covenant\ElpisClient\`
- **Plugin Install Path:** `F:\src3\Covenant\ElpisClient\Plugins\yes-ue-mcp\`
- **Version Control:** Perforce (plugin excluded via `.p4ignore`)
- **MCP Endpoint:** `http://127.0.0.1:8080/message`
- **Status:** Primary test environment

**Secondary Test Project:** GameAnimationSample56 (UE 5.6)
- **Project Path:** `F:\src_ue5\GameAnimationSample56\`
- **Plugin Install Path:** `F:\src_ue5\GameAnimationSample56\Plugins\yes-ue-mcp\`
- **MCP Endpoint:** `http://127.0.0.1:8081/message`
- **Status:** Active test environment for UE 5.6

## Module Structure

- **YesUeMcp** (Runtime) - Core MCP protocol layer
- **YesUeMcpEditor** (Editor) - HTTP server + tool implementations

## Coding Standards

- Follow Epic's C++ coding conventions
- Use `YESUEMCP_API` / `YESUEMCPEDITOR_API` for exported symbols
- All tools inherit from `UMcpToolBase`
- Register tools in `FYesUeMcpEditorModule::RegisterBuiltInTools()`

## Version Management

- **Plugin version is defined in `YesUeMcp.uplugin`** (`VersionName` field)
- **Increment version when:**
  - Adding a new tool
  - Changing MCP protocol behavior
  - Modifying tool input/output schema
  - Breaking changes to existing tools
- Use semantic versioning: `MAJOR.MINOR.PATCH`
  - MAJOR: Breaking changes
  - MINOR: New features (new tools, new parameters)
  - PATCH: Bug fixes

## Key Files

- `Source/YesUeMcp/Public/Tools/McpToolBase.h` - Base class for all tools
- `Source/YesUeMcp/Public/Tools/McpToolRegistry.h` - Tool registration
- `Source/YesUeMcpEditor/Public/Server/McpServer.h` - HTTP server
- `Source/YesUeMcpEditor/Public/Subsystem/McpEditorSubsystem.h` - Lifecycle management

## MCP Server

- **Protocol:** MCP 2024-11-05 with JSON-RPC 2.0
- **Transport:** HTTP (Streamable HTTP ready)
- **Endpoint:** `/message`
- **Port:** 8080 (configurable)
- **CORS:** Enabled for cross-origin requests

## Available Tools (19 total)

### Asset Tools
| Tool | Description |
|------|-------------|
| `search-assets` | Search assets by pattern, class, or path with wildcards |
| `inspect-asset` | General-purpose asset inspection using UE reflection (any asset type) |

### Blueprint Tools
| Tool | Description |
|------|-------------|
| `analyze-blueprint` | Complete blueprint structure analysis |
| `get-blueprint-functions` | Function signatures and metadata |
| `get-blueprint-variables` | Variable types and properties |
| `get-blueprint-components` | Component hierarchy with transforms |
| `get-blueprint-graph` | Read complete graph structure (nodes, pins, connections) |
| `get-blueprint-node` | Get detailed info about a specific node by GUID |
| `get-blueprint-defaults` | Read CDO property values (supports `include_inherited`) |
| `list-blueprint-callables` | List all events, functions, macros with metadata |
| `get-callable-details` | Get full graph for a specific callable |

### Level Tools
| Tool | Description |
|------|-------------|
| `query-level` | List actors in currently open level with filtering |
| `get-actor-details` | Inspect specific actor properties and components |

### Project Tools
| Tool | Description |
|------|-------------|
| `get-project-settings` | Query input, collision, tags, and map settings |
| `get-class-hierarchy` | Browse class inheritance (parents/children) |
| `inspect-data-asset` | Read DataTable and DataAsset contents |

### Reference Tools
| Tool | Description |
|------|-------------|
| `find-references` | Find references to assets, Blueprint variables, or nodes (type: asset/property/node) |

### Widget Tools
| Tool | Description |
|------|-------------|
| `inspect-widget-blueprint` | Inspect Widget Blueprint hierarchy, slots (anchors, offsets, sizes), visibility, and property bindings |

## Future Work

See GitHub Issues for planned features and enhancements.

### Potential Additions
- Animation asset query (AnimBP, montages, notifies)
- Material/shader graph inspection
- AI behavior tree analysis
- Write operations (spawn actors, modify properties)
- Streaming support for large datasets
- MCP Resources (expose project files)
- MCP Prompts (template code generation)

## Known Limitations

1. **Read-only** - No modification operations supported
2. **Editor-only** - Cannot run in packaged game
3. **Single session** - No multi-client support
4. **No streaming** - Synchronous responses only

## Project History

Historical development notes have been migrated to GitHub Issues for reference.
See: https://github.com/softdaddy-o/yes-ue-mcp-private/issues?q=label%3Adocumentation
