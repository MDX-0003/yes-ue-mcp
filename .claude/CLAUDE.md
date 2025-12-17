# yes-ue-mcp - Claude Instructions

**Native C++ MCP Plugin for Unreal Engine 5.4+**

## Project Overview

This plugin implements the Model Context Protocol (MCP) over Streamable HTTP, allowing AI assistants (Claude, Cursor, Windsurf, etc.) to interact with the Unreal Editor.

## Repository Structure

| Remote | Repository | Purpose |
|--------|------------|---------|
| `origin` | `yes-ue-mcp-private` | Development (private) |
| `public` | `yes-ue-mcp` | Release (public) |

## Git Workflow

### Private Repository (origin)
- **Commit frequently** - whenever one issue/task is finished
- Use descriptive commit messages
- Push to `origin` for development work

```bash
git add -A && git commit -m "feat: add blueprint analysis tool"
git push origin main
```

### Public Repository (public)
- **Commit aggregated changes** - only when a new version is finished
- Use version tags (v1.0.0, v1.1.0, etc.)
- Push to `public` for releases

```bash
git push public main
git tag v1.0.0
git push public --tags
```

## Module Structure

- **YesUeMcp** (Runtime) - Core MCP protocol layer
- **YesUeMcpEditor** (Editor) - HTTP server + tool implementations

## Coding Standards

- Follow Epic's C++ coding conventions
- Use `YESUEMCP_API` / `YESUEMCPEDITOR_API` for exported symbols
- All tools inherit from `UMcpToolBase`
- Register tools in `FYesUeMcpEditorModule::RegisterBuiltInTools()`

## Key Files

- `Source/YesUeMcp/Public/Tools/McpToolBase.h` - Base class for all tools
- `Source/YesUeMcp/Public/Tools/McpToolRegistry.h` - Tool registration
- `Source/YesUeMcpEditor/Public/Server/McpServer.h` - HTTP server
- `Source/YesUeMcpEditor/Public/Subsystem/McpEditorSubsystem.h` - Lifecycle management
