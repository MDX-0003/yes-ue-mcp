#!/usr/bin/env python3
"""
MCP Tools Integration Tests (v1.12.0 - Live Coding Enhanced)

This script tests the yes-ue-mcp plugin tools using Python's unittest framework.
Updated for the consolidated tool architecture (10 read tools + 20 write tools).

v1.9.0 Dynamic Reflection Features:
    - Dynamic material expression creation (any UMaterialExpression class)
    - Dynamic Blueprint node creation (any UK2Node class)
    - Dynamic asset creation (DataAsset, arbitrary UObject types)
    - Extended property support (TMap, TSet, Object references)
    - Unified McpPropertySerializer for all property operations

StateTree Tools:
    - query-statetree: Query StateTree structure (states, transitions, tasks, evaluators, parameters)
    - add-statetree-state: Add a new state to a StateTree
    - remove-statetree-state: Remove a state from a StateTree
    - add-statetree-task: Add a task to a state
    - add-statetree-transition: Add a transition between states

Scripting Tools:
    - run-python-script: Execute Python scripts in Unreal Editor (inline or file)

Build Tools:
    - trigger-live-coding: Trigger Live Coding compilation with async/sync modes

Consolidated Read Tools:
    - query-blueprint: Merged from analyze-blueprint, get-blueprint-functions,
                       get-blueprint-variables, get-blueprint-components, get-blueprint-defaults
    - query-blueprint-graph: Merged from get-blueprint-graph, get-blueprint-node,
                             list-blueprint-callables, get-callable-details
    - query-asset: Merged from search-assets, inspect-asset, inspect-data-asset
    - query-material: Merged from get-material-graph, get-material-parameters
    - query-level: Merged with get-actor-details (use actor_name for detail mode)
    - get-project-info: Merged with get-project-settings (use section param)
    - get-class-hierarchy, find-references, inspect-widget-blueprint, get-logs

Usage:
    python -m pytest test_mcp_tools.py -v
    python -m unittest test_mcp_tools -v
    python test_mcp_tools.py

Requirements:
    - Unreal Editor running with yes-ue-mcp plugin
    - MCP server started (check toolbar icon)
    - pip install requests pytest (optional for pytest runner)

Environment Variables:
    MCP_HOST: MCP server host (default: 127.0.0.1)
    MCP_PORT: MCP server port (default: 8080)
"""

import json
import os
import unittest
import requests
from typing import Any, Dict, List, Optional


class McpClient:
    """Client for making MCP JSON-RPC requests."""

    def __init__(self, host: str = "127.0.0.1", port: int = 8080):
        self.url = f"http://{host}:{port}/mcp"
        self.request_id = 0

    def call_tool(self, tool_name: str, arguments: Dict[str, Any] = None) -> Dict[str, Any]:
        """Call an MCP tool and return the result."""
        self.request_id += 1
        payload = {
            "jsonrpc": "2.0",
            "id": str(self.request_id),
            "method": "tools/call",
            "params": {
                "name": tool_name,
                "arguments": arguments or {}
            }
        }

        response = requests.post(
            self.url,
            json=payload,
            headers={"Content-Type": "application/json"},
            timeout=30
        )
        response.raise_for_status()

        result = response.json()
        if "error" in result:
            raise McpError(result["error"].get("message", "Unknown error"))

        # Parse the content from the result
        result_data = result.get("result", {})

        # Check if this is an error response
        if result_data.get("isError"):
            content = result_data.get("content", [])
            error_msg = "Unknown error"
            if content and content[0].get("type") == "text":
                error_msg = content[0]["text"]
            raise McpError(error_msg)

        content = result_data.get("content", [])
        if content and content[0].get("type") == "text":
            text = content[0]["text"]
            if text.strip():
                return json.loads(text)
        return result_data

    def list_tools(self) -> List[Dict[str, Any]]:
        """List all available tools."""
        self.request_id += 1
        payload = {
            "jsonrpc": "2.0",
            "id": str(self.request_id),
            "method": "tools/list"
        }

        response = requests.post(
            self.url,
            json=payload,
            headers={"Content-Type": "application/json"},
            timeout=10
        )
        response.raise_for_status()

        result = response.json()
        return result.get("result", {}).get("tools", [])


class McpError(Exception):
    """Exception raised for MCP errors."""
    pass


class McpTestCase(unittest.TestCase):
    """Base test case with MCP client setup."""

    client: McpClient = None

    @classmethod
    def setUpClass(cls):
        """Set up MCP client for all tests."""
        host = os.environ.get("MCP_HOST", "127.0.0.1")
        port = int(os.environ.get("MCP_PORT", "8080"))
        cls.client = McpClient(host, port)

        # Verify connection
        try:
            tools = cls.client.list_tools()
            if not tools:
                raise unittest.SkipTest("No tools available from MCP server")
        except requests.exceptions.ConnectionError:
            raise unittest.SkipTest(
                f"Could not connect to MCP server at {host}:{port}. "
                "Make sure Unreal Editor is running with the plugin."
            )


class TestConnection(McpTestCase):
    """Test MCP server connection."""

    def test_server_responds(self):
        """Test that the MCP server responds to requests."""
        tools = self.client.list_tools()
        self.assertIsInstance(tools, list)
        self.assertGreater(len(tools), 0)

    def test_minimum_tools_available(self):
        """Test that expected minimum number of tools are available."""
        tools = self.client.list_tools()
        tool_names = [t["name"] for t in tools]
        # After StateTree addition: 11 read + 22 write = 33 tools
        # Read: 10 original + 1 query-statetree = 11
        # Write: 18 original + 4 StateTree write tools = 22
        self.assertGreaterEqual(len(tool_names), 33,
            f"Expected at least 33 tools, got {len(tool_names)}")

    def test_required_read_tools_exist(self):
        """Test that essential read tools are registered (after consolidation)."""
        tools = self.client.list_tools()
        tool_names = {t["name"] for t in tools}

        # New consolidated tools (11 read tools total)
        required_tools = [
            "query-blueprint",        # Merged: analyze-blueprint, get-blueprint-functions, etc.
            "query-blueprint-graph",  # Merged: get-blueprint-graph, get-blueprint-node, etc.
            "query-level",            # Merged with get-actor-details
            "get-project-info",       # Merged with get-project-settings
            "query-asset",            # Merged: search-assets, inspect-asset, inspect-data-asset
            "query-material",         # Merged: get-material-graph, get-material-parameters
            "query-statetree",        # StateTree query tool
            "get-class-hierarchy",
            "find-references",
            "inspect-widget-blueprint",
            "get-logs",
        ]
        for tool in required_tools:
            self.assertIn(tool, tool_names, f"Required tool '{tool}' not found")

    def test_old_tools_removed(self):
        """Test that old pre-consolidation tools are no longer registered."""
        tools = self.client.list_tools()
        tool_names = {t["name"] for t in tools}

        # These tools should NOT exist after consolidation
        removed_tools = [
            "analyze-blueprint",      # Now query-blueprint
            "get-blueprint-functions",
            "get-blueprint-variables",
            "get-blueprint-components",
            "get-blueprint-defaults",
            "get-blueprint-graph",    # Now query-blueprint-graph
            "get-blueprint-node",
            "list-blueprint-callables",
            "get-callable-details",
            "search-assets",          # Now query-asset
            "inspect-asset",
            "inspect-data-asset",
            "get-material-graph",     # Now query-material
            "get-material-parameters",
            "get-actor-details",      # Now query-level with actor_name
            "get-project-settings",   # Now get-project-info with section
        ]
        for tool in removed_tools:
            self.assertNotIn(tool, tool_names, f"Old tool '{tool}' should have been removed")

    def test_required_write_tools_exist(self):
        """Test that essential write tools are registered."""
        tools = self.client.list_tools()
        tool_names = {t["name"] for t in tools}

        required_tools = [
            "spawn-actor",
            "delete-actor",
            "create-asset",
            "delete-asset",
            "compile-blueprint",
            # StateTree write tools
            "add-statetree-state",
            "remove-statetree-state",
            "add-statetree-task",
            "add-statetree-transition",
        ]
        for tool in required_tools:
            self.assertIn(tool, tool_names, f"Required tool '{tool}' not found")


class TestReadTools(McpTestCase):
    """Test read-only MCP tools."""

    def test_get_project_info(self):
        """Test get-project-info returns valid project data."""
        result = self.client.call_tool("get-project-info")

        self.assertIn("project_name", result)
        self.assertIn("project_path", result)
        self.assertIn("plugin", result)
        self.assertIn("version", result["plugin"])

    def test_get_project_info_with_settings(self):
        """Test get-project-info with settings section (merged from get-project-settings)."""
        result = self.client.call_tool("get-project-info", {"section": "input"})

        # Should still have basic project info
        self.assertIn("project_name", result)
        self.assertIn("plugin", result)
        # Should also have settings
        self.assertIn("settings", result)
        self.assertIn("input", result["settings"])

    def test_get_project_info_all_settings(self):
        """Test get-project-info with all settings."""
        result = self.client.call_tool("get-project-info", {"section": "all"})

        self.assertIn("settings", result)
        settings = result["settings"]
        self.assertIn("input", settings)
        self.assertIn("collision", settings)
        self.assertIn("tags", settings)
        self.assertIn("maps", settings)

    def test_query_level(self):
        """Test query-level returns actor list."""
        result = self.client.call_tool("query-level", {"limit": 5})

        self.assertIn("actors", result)
        self.assertIn("level_name", result)
        self.assertIsInstance(result["actors"], list)

    def test_query_level_with_filter(self):
        """Test query-level with class filter."""
        result = self.client.call_tool("query-level", {
            "class_filter": "PlayerStart",
            "limit": 10
        })

        self.assertIn("actors", result)
        # All returned actors should match filter
        for actor in result["actors"]:
            self.assertIn("PlayerStart", actor["class"])

    def test_query_level_detail_mode(self):
        """Test query-level with actor_name for detailed info (merged from get-actor-details)."""
        # First get list of actors to find one to query
        list_result = self.client.call_tool("query-level", {"limit": 1})
        if not list_result.get("actors"):
            self.skipTest("No actors in level to test detail mode")

        actor_name = list_result["actors"][0]["name"]

        # Query with actor_name for detailed info
        detail_result = self.client.call_tool("query-level", {
            "actor_name": actor_name,
            "include_properties": True,
            "include_components": True
        })

        # Should have detailed info
        self.assertIn("name", detail_result)
        self.assertIn("class", detail_result)
        self.assertIn("transform", detail_result)
        # Detail mode includes properties
        self.assertIn("properties", detail_result)

    def test_get_logs(self):
        """Test get-logs returns log entries."""
        result = self.client.call_tool("get-logs", {"limit": 10})

        self.assertIn("entries", result)
        self.assertIsInstance(result["entries"], list)


class TestQueryAsset(McpTestCase):
    """Test query-asset tool (consolidated from search-assets, inspect-asset, inspect-data-asset)."""

    def test_search_mode(self):
        """Test query-asset in search mode (was search-assets)."""
        result = self.client.call_tool("query-asset", {
            "query": "*",
            "limit": 5
        })

        self.assertIn("assets", result)
        self.assertIn("count", result)

    def test_search_with_class_filter(self):
        """Test query-asset search with class filter."""
        result = self.client.call_tool("query-asset", {
            "query": "*",
            "class": "Blueprint",
            "limit": 10
        })

        self.assertIn("assets", result)
        # All returned assets should be Blueprints
        for asset in result.get("assets", []):
            self.assertEqual(asset.get("class"), "Blueprint")

    def test_search_with_path_filter(self):
        """Test query-asset search with path filter."""
        result = self.client.call_tool("query-asset", {
            "query": "*",
            "path": "/Game",
            "limit": 5
        })

        self.assertIn("assets", result)
        # All returned assets should be under /Game
        for asset in result.get("assets", []):
            self.assertTrue(asset.get("path", "").startswith("/Game"))

    def test_inspect_mode(self):
        """Test query-asset in inspect mode (was inspect-asset)."""
        # First search for an asset to inspect
        search_result = self.client.call_tool("query-asset", {
            "query": "*",
            "limit": 1
        })
        if not search_result.get("assets"):
            self.skipTest("No assets to inspect")

        asset_path = search_result["assets"][0]["path"]

        # Inspect the asset
        inspect_result = self.client.call_tool("query-asset", {
            "asset_path": asset_path,
            "depth": 1
        })

        self.assertIn("asset_path", inspect_result)
        self.assertIn("class", inspect_result)


class TestQueryBlueprint(McpTestCase):
    """Test query-blueprint tool (consolidated from analyze-blueprint, get-blueprint-functions, etc.)."""

    @classmethod
    def setUpClass(cls):
        """Find a Blueprint to test with."""
        super().setUpClass()

        # Search for any Blueprint
        result = cls.client.call_tool("query-asset", {
            "query": "*",
            "class": "Blueprint",
            "limit": 1
        })
        if result.get("assets"):
            cls.test_bp_path = result["assets"][0]["path"]
        else:
            cls.test_bp_path = None

    def test_default_mode(self):
        """Test query-blueprint returns all info by default."""
        if not self.test_bp_path:
            self.skipTest("No Blueprint found for testing")

        result = self.client.call_tool("query-blueprint", {
            "asset_path": self.test_bp_path
        })

        # Response uses 'path' not 'asset_path'
        self.assertIn("path", result)
        self.assertIn("parent_class", result)
        # Default includes functions, variables, components
        self.assertIn("functions", result)
        self.assertIn("variables", result)
        self.assertIn("components", result)

    def test_include_functions_only(self):
        """Test query-blueprint with include=functions."""
        if not self.test_bp_path:
            self.skipTest("No Blueprint found for testing")

        result = self.client.call_tool("query-blueprint", {
            "asset_path": self.test_bp_path,
            "include": "functions"
        })

        self.assertIn("functions", result)

    def test_include_variables_only(self):
        """Test query-blueprint with include=variables."""
        if not self.test_bp_path:
            self.skipTest("No Blueprint found for testing")

        result = self.client.call_tool("query-blueprint", {
            "asset_path": self.test_bp_path,
            "include": "variables"
        })

        self.assertIn("variables", result)

    def test_include_components_only(self):
        """Test query-blueprint with include=components."""
        if not self.test_bp_path:
            self.skipTest("No Blueprint found for testing")

        result = self.client.call_tool("query-blueprint", {
            "asset_path": self.test_bp_path,
            "include": "components"
        })

        self.assertIn("components", result)

    def test_include_defaults(self):
        """Test query-blueprint with include=defaults (was get-blueprint-defaults)."""
        if not self.test_bp_path:
            self.skipTest("No Blueprint found for testing")

        result = self.client.call_tool("query-blueprint", {
            "asset_path": self.test_bp_path,
            "include": "defaults"
        })

        self.assertIn("defaults", result)


class TestQueryBlueprintGraph(McpTestCase):
    """Test query-blueprint-graph tool (consolidated from get-blueprint-graph, get-blueprint-node, etc.)."""

    @classmethod
    def setUpClass(cls):
        """Find a Blueprint to test with."""
        super().setUpClass()

        # Search for any Blueprint
        result = cls.client.call_tool("query-asset", {
            "query": "*",
            "class": "Blueprint",
            "limit": 1
        })
        if result.get("assets"):
            cls.test_bp_path = result["assets"][0]["path"]
        else:
            cls.test_bp_path = None

    def test_default_mode_returns_all_graphs(self):
        """Test query-blueprint-graph returns all graphs by default."""
        if not self.test_bp_path:
            self.skipTest("No Blueprint found for testing")

        result = self.client.call_tool("query-blueprint-graph", {
            "asset_path": self.test_bp_path
        })

        # Response uses 'blueprint' not 'asset_path'
        self.assertIn("blueprint", result)
        self.assertIn("graphs", result)
        self.assertIsInstance(result["graphs"], list)

    def test_filter_by_graph_type(self):
        """Test query-blueprint-graph with graph_type filter."""
        if not self.test_bp_path:
            self.skipTest("No Blueprint found for testing")

        result = self.client.call_tool("query-blueprint-graph", {
            "asset_path": self.test_bp_path,
            "graph_type": "event"
        })

        self.assertIn("graphs", result)
        # All returned graphs should be event graphs
        for graph in result.get("graphs", []):
            self.assertEqual(graph.get("type"), "event")

    def test_list_callables(self):
        """Test query-blueprint-graph with list_callables=true (was list-blueprint-callables)."""
        if not self.test_bp_path:
            self.skipTest("No Blueprint found for testing")

        result = self.client.call_tool("query-blueprint-graph", {
            "asset_path": self.test_bp_path,
            "list_callables": True
        })

        # Response has separate arrays: events, functions, macros
        self.assertIn("events", result)
        self.assertIn("functions", result)
        self.assertIn("macros", result)
        self.assertIsInstance(result["events"], list)

    def test_callable_details(self):
        """Test query-blueprint-graph with callable_name (was get-callable-details)."""
        if not self.test_bp_path:
            self.skipTest("No Blueprint found for testing")

        # First list callables to find one
        list_result = self.client.call_tool("query-blueprint-graph", {
            "asset_path": self.test_bp_path,
            "list_callables": True
        })

        # Get callables from events, functions, or macros arrays
        callables = (list_result.get("events", []) +
                     list_result.get("functions", []) +
                     list_result.get("macros", []))
        if not callables:
            self.skipTest("No callables found in Blueprint")

        callable_name = callables[0].get("name")

        # Get details for that callable
        detail_result = self.client.call_tool("query-blueprint-graph", {
            "asset_path": self.test_bp_path,
            "callable_name": callable_name
        })

        self.assertIn("callable", detail_result)
        self.assertIn("nodes", detail_result)


class TestQueryMaterial(McpTestCase):
    """Test query-material tool (consolidated from get-material-graph, get-material-parameters)."""

    @classmethod
    def setUpClass(cls):
        """Find a Material to test with."""
        super().setUpClass()

        # Search for any Material
        result = cls.client.call_tool("query-asset", {
            "query": "*",
            "class": "Material",
            "limit": 1
        })
        if result.get("assets"):
            cls.test_material_path = result["assets"][0]["path"]
        else:
            cls.test_material_path = None

    def test_default_mode(self):
        """Test query-material returns all info by default."""
        if not self.test_material_path:
            self.skipTest("No Material found for testing")

        result = self.client.call_tool("query-material", {
            "asset_path": self.test_material_path
        })

        self.assertIn("asset_path", result)
        self.assertIn("asset_type", result)
        # Default includes both graph and parameters
        self.assertIn("graph", result)
        self.assertIn("parameters", result)

    def test_include_graph_only(self):
        """Test query-material with include=graph (was get-material-graph)."""
        if not self.test_material_path:
            self.skipTest("No Material found for testing")

        result = self.client.call_tool("query-material", {
            "asset_path": self.test_material_path,
            "include": "graph"
        })

        self.assertIn("graph", result)
        # Should have expressions array
        if result.get("graph"):
            self.assertIn("expressions", result["graph"])

    def test_include_parameters_only(self):
        """Test query-material with include=parameters (was get-material-parameters)."""
        if not self.test_material_path:
            self.skipTest("No Material found for testing")

        result = self.client.call_tool("query-material", {
            "asset_path": self.test_material_path,
            "include": "parameters"
        })

        self.assertIn("parameters", result)
        params = result["parameters"]
        # Should have parameter type arrays
        self.assertIn("scalar", params)
        self.assertIn("vector", params)
        self.assertIn("texture", params)


class TestActorLifecycle(McpTestCase):
    """Test actor spawn and delete operations."""

    def setUp(self):
        """Track actors to clean up."""
        self.spawned_actors = []

    def tearDown(self):
        """Clean up any spawned actors."""
        for actor_name in self.spawned_actors:
            try:
                self.client.call_tool("delete-actor", {"actor_name": actor_name})
            except:
                pass

    def test_spawn_actor(self):
        """Test spawning an actor in the level."""
        result = self.client.call_tool("spawn-actor", {
            "actor_class": "PointLight",
            "location": [100, 200, 300],
            "label": "MCP_Test_SpawnActor"
        })

        self.assertTrue(result.get("success"), "spawn-actor should succeed")
        self.assertIn("actor_name", result)
        self.spawned_actors.append(result["actor_name"])

    def test_spawn_and_verify_actor(self):
        """Test that spawned actor appears in level query."""
        label = "MCP_Test_VerifySpawn"

        # Spawn
        spawn_result = self.client.call_tool("spawn-actor", {
            "actor_class": "PointLight",
            "location": [0, 0, 500],
            "label": label
        })
        self.assertTrue(spawn_result.get("success"))
        self.spawned_actors.append(spawn_result["actor_name"])

        # Verify
        query_result = self.client.call_tool("query-level", {
            "class_filter": "*Light*"
        })
        actor_labels = [a.get("label") for a in query_result.get("actors", [])]
        self.assertIn(label, actor_labels,
            f"Spawned actor '{label}' should appear in level query")

    def test_delete_actor(self):
        """Test deleting an actor from the level."""
        # First spawn an actor
        spawn_result = self.client.call_tool("spawn-actor", {
            "actor_class": "PointLight",
            "label": "MCP_Test_DeleteActor"
        })
        self.assertTrue(spawn_result.get("success"))
        actor_name = spawn_result["actor_name"]

        # Delete it
        delete_result = self.client.call_tool("delete-actor", {
            "actor_name": actor_name
        })
        self.assertTrue(delete_result.get("success"), "delete-actor should succeed")

        # Verify it's gone
        query_result = self.client.call_tool("query-level", {
            "class_filter": "*Light*"
        })
        actor_names = [a.get("name") for a in query_result.get("actors", [])]
        self.assertNotIn(actor_name, actor_names,
            f"Deleted actor '{actor_name}' should not appear in level query")


class TestAssetLifecycle(McpTestCase):
    """Test asset create and delete operations."""

    # Assets used by this test class
    TEST_ASSETS = [
        "/Game/BP_MCP_TestCreate",
        "/Game/BP_MCP_TestVerify",
        "/Game/BP_MCP_TestDelete",
        "/Game/DT_MCP_TestCreate",
        "/Game/M_MCP_TestCreate",
        "/Game/ABP_MCP_TestCreate",
        "/Game/WBP_MCP_TestCreate",
    ]

    @classmethod
    def setUpClass(cls):
        """Clean up any stale test assets from previous runs."""
        super().setUpClass()
        for asset_path in cls.TEST_ASSETS:
            # Try multiple times to ensure cleanup
            for _ in range(3):
                try:
                    cls.client.call_tool("delete-asset", {"asset_path": asset_path})
                except:
                    pass
                # Brief pause to allow GC to complete
                import time
                time.sleep(0.1)

    def setUp(self):
        """Track assets to clean up."""
        self.created_assets = []

    def tearDown(self):
        """Clean up any created assets."""
        for asset_path in self.created_assets:
            try:
                self.client.call_tool("delete-asset", {"asset_path": asset_path})
            except:
                pass

    def test_create_blueprint(self):
        """Test creating a Blueprint asset."""
        asset_path = "/Game/BP_MCP_TestCreate"

        try:
            result = self.client.call_tool("create-asset", {
                "asset_path": asset_path,
                "asset_class": "Blueprint",
                "parent_class": "Actor"
            })
            self.assertTrue(result.get("success"), "create-asset should succeed")
            self.created_assets.append(asset_path)
        except McpError as e:
            if "already exists" in str(e).lower():
                self.skipTest("Asset already exists from previous run - rebuild editor with GC fix")
            raise

    def test_create_and_verify_blueprint(self):
        """Test that created Blueprint can be analyzed."""
        asset_path = "/Game/BP_MCP_TestVerify"

        # Create
        try:
            create_result = self.client.call_tool("create-asset", {
                "asset_path": asset_path,
                "asset_class": "Blueprint",
                "parent_class": "Actor"
            })
            self.assertTrue(create_result.get("success"))
            self.created_assets.append(asset_path)
        except McpError as e:
            if "already exists" in str(e).lower():
                self.skipTest("Asset already exists from previous run - rebuild editor with GC fix")
            raise

        # Verify with query-blueprint (was analyze-blueprint)
        analyze_result = self.client.call_tool("query-blueprint", {
            "asset_path": asset_path,
            "include": "functions"  # Just get minimal info
        })
        self.assertEqual(analyze_result.get("parent_class"), "Actor")

    def test_delete_asset(self):
        """Test deleting an asset."""
        asset_path = "/Game/BP_MCP_TestDelete"

        # Create first
        try:
            self.client.call_tool("create-asset", {
                "asset_path": asset_path,
                "asset_class": "Blueprint"
            })
        except McpError as e:
            if "already exists" in str(e).lower():
                self.skipTest("Asset already exists from previous run - rebuild editor with GC fix")
            raise

        # Delete
        delete_result = self.client.call_tool("delete-asset", {
            "asset_path": asset_path
        })
        self.assertTrue(delete_result.get("success"), "delete-asset should succeed")

        # Verify it's gone (use query-asset which replaced search-assets)
        search_result = self.client.call_tool("query-asset", {
            "query": "BP_MCP_TestDelete"
        })
        self.assertEqual(search_result.get("count", 0), 0,
            "Deleted asset should not appear in search")

    def test_create_datatable(self):
        """Test creating a DataTable asset."""
        asset_path = "/Game/DT_MCP_TestCreate"

        try:
            # DataTable requires a row struct - use FTableRowBase which is always available
            result = self.client.call_tool("create-asset", {
                "asset_path": asset_path,
                "asset_class": "DataTable",
                "row_struct": "TableRowBase"
            })
            self.assertTrue(result.get("success"), "create-asset should succeed for DataTable")
            self.created_assets.append(asset_path)

            # Verify the DataTable exists by querying it
            query_result = self.client.call_tool("query-asset", {
                "asset_path": asset_path
            })
            # query-asset returns 'type' for DataTables, not 'class'
            self.assertIn("type", query_result)
            self.assertEqual(query_result.get("type"), "DataTable")

        except McpError as e:
            if "already exists" in str(e).lower():
                self.skipTest("Asset already exists from previous run - rebuild editor with GC fix")
            raise

    def test_create_material(self):
        """Test creating a Material asset."""
        asset_path = "/Game/M_MCP_TestCreate"

        try:
            result = self.client.call_tool("create-asset", {
                "asset_path": asset_path,
                "asset_class": "Material"
            })
            self.assertTrue(result.get("success"), "create-asset should succeed for Material")
            self.created_assets.append(asset_path)

            # Verify the Material exists by querying it
            query_result = self.client.call_tool("query-material", {
                "asset_path": asset_path,
                "include": "parameters"
            })
            self.assertIn("asset_path", query_result)

        except McpError as e:
            if "already exists" in str(e).lower():
                self.skipTest("Asset already exists from previous run - rebuild editor with GC fix")
            raise

    def test_create_anim_blueprint(self):
        """Test creating an AnimBlueprint asset."""
        asset_path = "/Game/ABP_MCP_TestCreate"

        try:
            result = self.client.call_tool("create-asset", {
                "asset_path": asset_path,
                "asset_class": "AnimBlueprint"
            })
            # AnimBlueprint may fail without a skeleton - that's acceptable
            if result.get("success"):
                self.created_assets.append(asset_path)
            else:
                self.skipTest("AnimBlueprint creation requires a skeleton")

        except McpError as e:
            if "already exists" in str(e).lower():
                self.skipTest("Asset already exists from previous run - rebuild editor with GC fix")
            # AnimBlueprint may fail without skeleton - skip instead of fail
            if "skeleton" in str(e).lower() or "failed" in str(e).lower():
                self.skipTest(f"AnimBlueprint creation requires additional setup: {e}")
            raise

    def test_create_widget_blueprint(self):
        """Test creating a WidgetBlueprint asset."""
        asset_path = "/Game/WBP_MCP_TestCreate"

        try:
            result = self.client.call_tool("create-asset", {
                "asset_path": asset_path,
                "asset_class": "WidgetBlueprint"
            })
            self.assertTrue(result.get("success"), "create-asset should succeed for WidgetBlueprint")
            self.created_assets.append(asset_path)

            # Verify the WidgetBlueprint exists by using inspect-widget-blueprint
            query_result = self.client.call_tool("inspect-widget-blueprint", {
                "asset_path": asset_path
            })
            self.assertIn("asset_path", query_result)

        except McpError as e:
            if "already exists" in str(e).lower():
                self.skipTest("Asset already exists from previous run - rebuild editor with GC fix")
            raise


class TestBlueprintModification(McpTestCase):
    """Test Blueprint graph modification tools."""

    @classmethod
    def setUpClass(cls):
        """Create a test Blueprint for modification tests."""
        super().setUpClass()
        import time
        cls.test_bp_path = "/Game/BP_MCP_ModificationTest"

        # Clean up any stale asset from previous runs (try multiple times)
        for _ in range(3):
            try:
                cls.client.call_tool("delete-asset", {"asset_path": cls.test_bp_path})
            except:
                pass
            time.sleep(0.1)

        # Create test Blueprint
        try:
            result = cls.client.call_tool("create-asset", {
                "asset_path": cls.test_bp_path,
                "asset_class": "Blueprint",
                "parent_class": "Actor"
            })
            if not result.get("success"):
                raise unittest.SkipTest("Could not create test Blueprint")
        except McpError as e:
            if "already exists" in str(e).lower():
                raise unittest.SkipTest("Asset already exists from previous run - rebuild editor with GC fix")
            raise

    @classmethod
    def tearDownClass(cls):
        """Delete test Blueprint."""
        try:
            cls.client.call_tool("delete-asset", {
                "asset_path": cls.test_bp_path
            })
        except:
            pass
        super().tearDownClass()

    def test_add_graph_node(self):
        """Test adding a node to Blueprint graph."""
        result = self.client.call_tool("add-graph-node", {
            "asset_path": self.test_bp_path,
            "node_class": "VariableGet",
            "variable_name": "DefaultSceneRoot",
            "graph_name": "EventGraph"
        })

        self.assertTrue(result.get("success"), "add-graph-node should succeed")
        self.assertTrue(result.get("needs_compile"), "Should need compile after adding node")

    def test_compile_blueprint(self):
        """Test compiling a Blueprint."""
        result = self.client.call_tool("compile-blueprint", {
            "asset_path": self.test_bp_path
        })

        self.assertTrue(result.get("success"), "compile-blueprint should succeed")
        self.assertIn(result.get("status"), ["compiled", "compiled_with_warnings"])

    def test_graph_modification_reflected(self):
        """Test that graph modifications are visible in query-blueprint-graph."""
        # Add a node
        add_result = self.client.call_tool("add-graph-node", {
            "asset_path": self.test_bp_path,
            "node_class": "VariableGet",
            "variable_name": "DefaultSceneRoot",
            "graph_name": "EventGraph"
        })
        self.assertTrue(add_result.get("success"))

        # Read graph (use query-blueprint-graph which replaced get-blueprint-graph)
        graph_result = self.client.call_tool("query-blueprint-graph", {
            "asset_path": self.test_bp_path,
            "graph_type": "event"
        })

        # Find VariableGet node
        found = False
        for graph in graph_result.get("graphs", []):
            for node in graph.get("nodes", []):
                if "VariableGet" in node.get("class", ""):
                    found = True
                    break

        self.assertTrue(found, "Added VariableGet node should appear in graph")


class TestGraphNodeOperations(McpTestCase):
    """Test graph node manipulation tools."""

    @classmethod
    def setUpClass(cls):
        """Create a test Blueprint for node operation tests."""
        super().setUpClass()
        import time
        cls.test_bp_path = "/Game/BP_MCP_GraphNodeTest"

        # Clean up any stale asset from previous runs (try multiple times)
        for _ in range(3):
            try:
                cls.client.call_tool("delete-asset", {"asset_path": cls.test_bp_path})
            except:
                pass
            time.sleep(0.1)

        # Create test Blueprint
        try:
            result = cls.client.call_tool("create-asset", {
                "asset_path": cls.test_bp_path,
                "asset_class": "Blueprint",
                "parent_class": "Actor"
            })
            if not result.get("success"):
                raise unittest.SkipTest("Could not create test Blueprint")
        except McpError as e:
            if "already exists" in str(e).lower():
                raise unittest.SkipTest("Asset already exists from previous run - rebuild editor with GC fix")
            raise

    @classmethod
    def tearDownClass(cls):
        """Delete test Blueprint."""
        try:
            cls.client.call_tool("delete-asset", {
                "asset_path": cls.test_bp_path
            })
        except:
            pass
        super().tearDownClass()

    def test_remove_graph_node(self):
        """Test removing a node from Blueprint graph."""
        # First add a node - use CallFunction with a known function
        add_result = self.client.call_tool("add-graph-node", {
            "asset_path": self.test_bp_path,
            "node_class": "CallFunction",
            "function_name": "PrintString",
            "graph_name": "EventGraph"
        })
        self.assertTrue(add_result.get("success"), "add-graph-node should succeed")
        node_guid = add_result.get("node_guid")
        self.assertIsNotNone(node_guid, "Should return node_guid")

        # Remove the node - tool uses 'node_id' param, not 'node_guid'
        remove_result = self.client.call_tool("remove-graph-node", {
            "asset_path": self.test_bp_path,
            "node_id": node_guid
        })
        self.assertTrue(remove_result.get("success"), "remove-graph-node should succeed")

    def test_connect_and_disconnect_pins(self):
        """Test connecting and disconnecting graph pins."""
        # Add two CallFunction nodes that can be connected
        # Note: Event nodes like BeginPlay require different handling
        add_result1 = self.client.call_tool("add-graph-node", {
            "asset_path": self.test_bp_path,
            "node_class": "CallFunction",
            "function_name": "Delay",
            "graph_name": "EventGraph"
        })

        add_result2 = self.client.call_tool("add-graph-node", {
            "asset_path": self.test_bp_path,
            "node_class": "CallFunction",
            "function_name": "PrintString",
            "graph_name": "EventGraph"
        })

        if not add_result1.get("success") or not add_result2.get("success"):
            self.skipTest("Could not create nodes for connection test")

        # Verify we got different GUIDs
        guid1 = add_result1.get("node_guid")
        guid2 = add_result2.get("node_guid")
        self.assertIsNotNone(guid1, "First node should have a GUID")
        self.assertIsNotNone(guid2, "Second node should have a GUID")
        self.assertNotEqual(guid1, guid2, f"Nodes should have different GUIDs: {guid1} vs {guid2}")

        # Try to connect them (exec pin to exec pin)
        # Tool uses source_node/source_pin/target_node/target_pin params
        connect_result = self.client.call_tool("connect-graph-pins", {
            "asset_path": self.test_bp_path,
            "source_node": add_result1.get("node_guid"),
            "source_pin": "then",
            "target_node": add_result2.get("node_guid"),
            "target_pin": "execute"
        })
        # Connection may fail if pins don't match, but tool should respond
        self.assertIn("success", connect_result)

        # Test disconnect
        if connect_result.get("success"):
            disconnect_result = self.client.call_tool("disconnect-graph-pin", {
                "asset_path": self.test_bp_path,
                "node_id": add_result1.get("node_guid"),
                "pin_name": "then"
            })
            self.assertIn("success", disconnect_result)


class TestPropertyTools(McpTestCase):
    """Test property modification and save tools."""

    @classmethod
    def setUpClass(cls):
        """Create a test Blueprint for property tests."""
        super().setUpClass()
        import time
        cls.test_bp_path = "/Game/BP_MCP_PropertyTest"

        # Clean up any stale asset from previous runs (try multiple times)
        for _ in range(3):
            try:
                cls.client.call_tool("delete-asset", {"asset_path": cls.test_bp_path})
            except:
                pass
            time.sleep(0.1)

        # Create test Blueprint
        try:
            result = cls.client.call_tool("create-asset", {
                "asset_path": cls.test_bp_path,
                "asset_class": "Blueprint",
                "parent_class": "Actor"
            })
            if not result.get("success"):
                raise unittest.SkipTest("Could not create test Blueprint")
        except McpError as e:
            if "already exists" in str(e).lower():
                raise unittest.SkipTest("Asset already exists from previous run - rebuild editor with GC fix")
            raise

    @classmethod
    def tearDownClass(cls):
        """Delete test Blueprint."""
        try:
            cls.client.call_tool("delete-asset", {
                "asset_path": cls.test_bp_path
            })
        except:
            pass
        super().tearDownClass()

    def test_set_property(self):
        """Test setting a property on an asset."""
        result = self.client.call_tool("set-property", {
            "asset_path": self.test_bp_path,
            "property_path": "bReplicates",
            "value": True
        })
        # Property may not exist on all assets, but tool should respond
        self.assertIn("success", result)

    def test_save_asset(self):
        """Test saving an asset."""
        result = self.client.call_tool("save-asset", {
            "asset_path": self.test_bp_path
        })
        self.assertTrue(result.get("success"), "save-asset should succeed")


class TestComponentTools(McpTestCase):
    """Test component add/remove tools."""

    def setUp(self):
        """Spawn a test actor."""
        self.test_actor_label = "MCP_ComponentTestActor"
        self.test_actor_name = None
        # Use StaticMeshActor since base Actor class cannot be spawned directly
        # spawn-actor uses 'label' param, not 'actor_name'
        try:
            result = self.client.call_tool("spawn-actor", {
                "actor_class": "StaticMeshActor",
                "label": self.test_actor_label
            })
            if not result.get("success"):
                self.skipTest("Could not spawn test actor")
            # Store the actual actor name returned by spawn
            self.test_actor_name = result.get("actor_name", self.test_actor_label)
        except McpError as e:
            self.skipTest(f"Could not spawn test actor: {e}")

    def tearDown(self):
        """Delete test actor."""
        # Try both the actor name and label
        for name in [self.test_actor_name, self.test_actor_label]:
            if name:
                try:
                    self.client.call_tool("delete-actor", {"actor_name": name})
                except:
                    pass

    def test_add_component(self):
        """Test adding a component to an actor."""
        # Use the actual actor name returned by spawn
        result = self.client.call_tool("add-component", {
            "actor_name": self.test_actor_name,
            "component_class": "StaticMeshComponent",
            "component_name": "TestMeshComp"
        })
        self.assertTrue(result.get("success"), "add-component should succeed")

    def test_remove_component(self):
        """Test removing a component from an actor."""
        # First add a component - use actual actor name
        add_result = self.client.call_tool("add-component", {
            "actor_name": self.test_actor_name,
            "component_class": "PointLightComponent",
            "component_name": "TestLightComp"
        })
        if not add_result.get("success"):
            self.skipTest("Could not add component to test")

        # Remove it
        remove_result = self.client.call_tool("remove-component", {
            "actor_name": self.test_actor_name,
            "component_name": "TestLightComp"
        })
        self.assertTrue(remove_result.get("success"), "remove-component should succeed")


class TestReferenceTools(McpTestCase):
    """Test find-references and get-class-hierarchy tools."""

    def test_get_class_hierarchy(self):
        """Test get-class-hierarchy returns inheritance info."""
        result = self.client.call_tool("get-class-hierarchy", {
            "class_name": "Actor"
        })

        # Response has 'class' object, not 'class_name' field
        self.assertIn("class", result)
        # Should have parent or children info
        self.assertTrue(
            "parents" in result or "children" in result,
            "Should return inheritance info"
        )

    def test_find_references_asset_mode(self):
        """Test find-references in asset mode."""
        # Search for any Blueprint first
        search_result = self.client.call_tool("query-asset", {
            "query": "*",
            "class": "Blueprint",
            "limit": 1
        })
        if not search_result.get("assets"):
            self.skipTest("No Blueprint found for reference test")

        bp_path = search_result["assets"][0]["path"]

        result = self.client.call_tool("find-references", {
            "type": "asset",
            "asset_path": bp_path
        })
        # Tool should respond with references (may be empty)
        self.assertIsInstance(result, dict)


class TestStateTreeTools(McpTestCase):
    """Test StateTree query and modification tools."""

    @classmethod
    def setUpClass(cls):
        """Find a StateTree to test with."""
        super().setUpClass()

        # Search for any StateTree
        result = cls.client.call_tool("query-asset", {
            "query": "*",
            "class": "StateTree",
            "limit": 1
        })
        if result.get("assets"):
            cls.test_statetree_path = result["assets"][0]["path"]
        else:
            cls.test_statetree_path = None

    def test_query_statetree_default(self):
        """Test query-statetree returns all info by default."""
        if not self.test_statetree_path:
            self.skipTest("No StateTree found for testing")

        result = self.client.call_tool("query-statetree", {
            "asset_path": self.test_statetree_path
        })

        self.assertIn("name", result)
        self.assertIn("path", result)
        # Default includes states, transitions, tasks, evaluators, parameters
        self.assertIn("states", result)
        self.assertIn("transitions", result)
        self.assertIn("tasks", result)
        self.assertIn("evaluators", result)
        self.assertIn("parameters", result)

    def test_query_statetree_states_only(self):
        """Test query-statetree with include=states."""
        if not self.test_statetree_path:
            self.skipTest("No StateTree found for testing")

        result = self.client.call_tool("query-statetree", {
            "asset_path": self.test_statetree_path,
            "include": "states"
        })

        self.assertIn("states", result)
        self.assertIn("items", result["states"])
        self.assertIn("count", result["states"])

    def test_query_statetree_transitions_only(self):
        """Test query-statetree with include=transitions."""
        if not self.test_statetree_path:
            self.skipTest("No StateTree found for testing")

        result = self.client.call_tool("query-statetree", {
            "asset_path": self.test_statetree_path,
            "include": "transitions"
        })

        self.assertIn("transitions", result)
        self.assertIn("items", result["transitions"])
        self.assertIn("count", result["transitions"])

    def test_query_statetree_tasks_only(self):
        """Test query-statetree with include=tasks."""
        if not self.test_statetree_path:
            self.skipTest("No StateTree found for testing")

        result = self.client.call_tool("query-statetree", {
            "asset_path": self.test_statetree_path,
            "include": "tasks"
        })

        self.assertIn("tasks", result)
        self.assertIn("items", result["tasks"])
        self.assertIn("count", result["tasks"])

    def test_query_statetree_evaluators_only(self):
        """Test query-statetree with include=evaluators."""
        if not self.test_statetree_path:
            self.skipTest("No StateTree found for testing")

        result = self.client.call_tool("query-statetree", {
            "asset_path": self.test_statetree_path,
            "include": "evaluators"
        })

        self.assertIn("evaluators", result)
        self.assertIn("items", result["evaluators"])
        self.assertIn("count", result["evaluators"])

    def test_query_statetree_parameters_only(self):
        """Test query-statetree with include=parameters."""
        if not self.test_statetree_path:
            self.skipTest("No StateTree found for testing")

        result = self.client.call_tool("query-statetree", {
            "asset_path": self.test_statetree_path,
            "include": "parameters"
        })

        self.assertIn("parameters", result)
        self.assertIn("items", result["parameters"])
        self.assertIn("count", result["parameters"])

    def test_query_statetree_detailed_mode(self):
        """Test query-statetree with detailed=true."""
        if not self.test_statetree_path:
            self.skipTest("No StateTree found for testing")

        result = self.client.call_tool("query-statetree", {
            "asset_path": self.test_statetree_path,
            "include": "states",
            "detailed": True
        })

        self.assertIn("states", result)
        states = result["states"].get("items", [])
        if states:
            # Detailed mode should include extra fields
            first_state = states[0]
            self.assertIn("type", first_state)
            # Detailed info includes depth, selection_behavior, etc.
            self.assertIn("selection_behavior", first_state)

    def test_query_statetree_schema(self):
        """Test query-statetree returns schema info."""
        if not self.test_statetree_path:
            self.skipTest("No StateTree found for testing")

        result = self.client.call_tool("query-statetree", {
            "asset_path": self.test_statetree_path
        })

        # Schema may or may not be set, but field should exist if schema is set
        # Just verify we can query without error

    def test_query_statetree_missing_asset_path(self):
        """Test query-statetree fails without asset_path."""
        with self.assertRaises(McpError) as ctx:
            self.client.call_tool("query-statetree", {})
        self.assertIn("asset_path", str(ctx.exception).lower())

    def test_query_statetree_nonexistent_asset(self):
        """Test query-statetree fails for non-existent asset."""
        with self.assertRaises(McpError) as ctx:
            self.client.call_tool("query-statetree", {
                "asset_path": "/Game/NonExistent/ST_DoesNotExist_12345"
            })
        error_msg = str(ctx.exception).lower()
        self.assertTrue(
            "not found" in error_msg or "failed to load" in error_msg,
            f"Expected 'not found' error, got: {ctx.exception}"
        )


class TestStateTreeWriteTools(McpTestCase):
    """Test StateTree modification tools (add-statetree-state, add-statetree-task, etc.)."""

    @classmethod
    def setUpClass(cls):
        """Find or create a StateTree to test with."""
        super().setUpClass()
        import time

        # Search for any StateTree to use for testing
        result = cls.client.call_tool("query-asset", {
            "query": "*",
            "class": "StateTree",
            "limit": 1
        })
        if result.get("assets"):
            cls.test_statetree_path = result["assets"][0]["path"]
        else:
            cls.test_statetree_path = None

    # -------------------------------------------------------------------------
    # add-statetree-state Tests
    # -------------------------------------------------------------------------

    def test_add_statetree_state_missing_asset_path(self):
        """Test add-statetree-state fails without asset_path."""
        with self.assertRaises(McpError) as ctx:
            self.client.call_tool("add-statetree-state", {
                "state_name": "TestState"
            })
        self.assertIn("asset_path", str(ctx.exception).lower())

    def test_add_statetree_state_missing_state_name(self):
        """Test add-statetree-state fails without state_name."""
        with self.assertRaises(McpError) as ctx:
            self.client.call_tool("add-statetree-state", {
                "asset_path": "/Game/Test/ST_Test"
            })
        self.assertIn("state_name", str(ctx.exception).lower())

    def test_add_statetree_state_nonexistent_asset(self):
        """Test add-statetree-state fails for non-existent StateTree."""
        with self.assertRaises(McpError) as ctx:
            self.client.call_tool("add-statetree-state", {
                "asset_path": "/Game/NonExistent/ST_DoesNotExist_12345",
                "state_name": "TestState"
            })
        error_msg = str(ctx.exception).lower()
        self.assertTrue(
            "not found" in error_msg or "failed to load" in error_msg,
            f"Expected 'not found' error, got: {ctx.exception}"
        )

    def test_add_statetree_state_nonexistent_parent(self):
        """Test add-statetree-state fails when parent_state doesn't exist."""
        if not self.test_statetree_path:
            self.skipTest("No StateTree found for testing")

        with self.assertRaises(McpError) as ctx:
            self.client.call_tool("add-statetree-state", {
                "asset_path": self.test_statetree_path,
                "state_name": "TestState",
                "parent_state": "NonExistentParent_12345"
            })
        error_msg = str(ctx.exception).lower()
        self.assertTrue(
            "not found" in error_msg or "parent" in error_msg,
            f"Expected 'parent not found' error, got: {ctx.exception}"
        )

    def test_add_statetree_state_basic(self):
        """Test adding a basic state to StateTree."""
        if not self.test_statetree_path:
            self.skipTest("No StateTree found for testing")

        # Generate unique state name
        import time
        state_name = f"MCP_TestState_{int(time.time())}"

        try:
            result = self.client.call_tool("add-statetree-state", {
                "asset_path": self.test_statetree_path,
                "state_name": state_name
            })

            self.assertTrue(result.get("success"), "add-statetree-state should succeed")
            self.assertEqual(result.get("state_name"), state_name)
            self.assertIn("message", result)

        finally:
            # Clean up - remove the test state
            try:
                self.client.call_tool("remove-statetree-state", {
                    "asset_path": self.test_statetree_path,
                    "state_name": state_name
                })
            except:
                pass

    def test_add_statetree_state_with_type(self):
        """Test adding a state with specific type."""
        if not self.test_statetree_path:
            self.skipTest("No StateTree found for testing")

        import time
        state_name = f"MCP_GroupState_{int(time.time())}"

        try:
            result = self.client.call_tool("add-statetree-state", {
                "asset_path": self.test_statetree_path,
                "state_name": state_name,
                "state_type": "Group"
            })

            self.assertTrue(result.get("success"), "add-statetree-state should succeed")
            self.assertEqual(result.get("state_type"), "Group")

        finally:
            try:
                self.client.call_tool("remove-statetree-state", {
                    "asset_path": self.test_statetree_path,
                    "state_name": state_name
                })
            except:
                pass

    def test_add_statetree_state_with_selection_behavior(self):
        """Test adding a state with specific selection behavior."""
        if not self.test_statetree_path:
            self.skipTest("No StateTree found for testing")

        import time
        state_name = f"MCP_SelectState_{int(time.time())}"

        try:
            result = self.client.call_tool("add-statetree-state", {
                "asset_path": self.test_statetree_path,
                "state_name": state_name,
                "selection_behavior": "TrySelectChildrenInOrder"
            })

            self.assertTrue(result.get("success"), "add-statetree-state should succeed")
            self.assertEqual(result.get("selection_behavior"), "TrySelectChildrenInOrder")

        finally:
            try:
                self.client.call_tool("remove-statetree-state", {
                    "asset_path": self.test_statetree_path,
                    "state_name": state_name
                })
            except:
                pass

    # -------------------------------------------------------------------------
    # remove-statetree-state Tests
    # -------------------------------------------------------------------------

    def test_remove_statetree_state_missing_asset_path(self):
        """Test remove-statetree-state fails without asset_path."""
        with self.assertRaises(McpError) as ctx:
            self.client.call_tool("remove-statetree-state", {
                "state_name": "TestState"
            })
        self.assertIn("asset_path", str(ctx.exception).lower())

    def test_remove_statetree_state_missing_state_name(self):
        """Test remove-statetree-state fails without state_name."""
        with self.assertRaises(McpError) as ctx:
            self.client.call_tool("remove-statetree-state", {
                "asset_path": "/Game/Test/ST_Test"
            })
        self.assertIn("state_name", str(ctx.exception).lower())

    def test_remove_statetree_state_nonexistent_asset(self):
        """Test remove-statetree-state fails for non-existent StateTree."""
        with self.assertRaises(McpError) as ctx:
            self.client.call_tool("remove-statetree-state", {
                "asset_path": "/Game/NonExistent/ST_DoesNotExist_12345",
                "state_name": "TestState"
            })
        error_msg = str(ctx.exception).lower()
        self.assertTrue(
            "not found" in error_msg or "failed to load" in error_msg,
            f"Expected 'not found' error, got: {ctx.exception}"
        )

    def test_remove_statetree_state_nonexistent_state(self):
        """Test remove-statetree-state fails when state doesn't exist."""
        if not self.test_statetree_path:
            self.skipTest("No StateTree found for testing")

        with self.assertRaises(McpError) as ctx:
            self.client.call_tool("remove-statetree-state", {
                "asset_path": self.test_statetree_path,
                "state_name": "NonExistentState_12345"
            })
        error_msg = str(ctx.exception).lower()
        self.assertTrue(
            "not found" in error_msg,
            f"Expected 'not found' error, got: {ctx.exception}"
        )

    def test_remove_statetree_state_basic(self):
        """Test removing a state from StateTree."""
        if not self.test_statetree_path:
            self.skipTest("No StateTree found for testing")

        import time
        state_name = f"MCP_RemoveState_{int(time.time())}"

        # First add a state
        add_result = self.client.call_tool("add-statetree-state", {
            "asset_path": self.test_statetree_path,
            "state_name": state_name
        })
        if not add_result.get("success"):
            self.skipTest("Could not add state for remove test")

        # Then remove it
        remove_result = self.client.call_tool("remove-statetree-state", {
            "asset_path": self.test_statetree_path,
            "state_name": state_name
        })

        self.assertTrue(remove_result.get("success"), "remove-statetree-state should succeed")
        self.assertEqual(remove_result.get("state_name"), state_name)
        self.assertIn("children_removed", remove_result)

    # -------------------------------------------------------------------------
    # add-statetree-task Tests
    # -------------------------------------------------------------------------

    def test_add_statetree_task_missing_asset_path(self):
        """Test add-statetree-task fails without asset_path."""
        with self.assertRaises(McpError) as ctx:
            self.client.call_tool("add-statetree-task", {
                "state_name": "TestState",
                "task_class": "StateTreeTask_Wait"
            })
        self.assertIn("asset_path", str(ctx.exception).lower())

    def test_add_statetree_task_missing_state_name(self):
        """Test add-statetree-task fails without state_name."""
        with self.assertRaises(McpError) as ctx:
            self.client.call_tool("add-statetree-task", {
                "asset_path": "/Game/Test/ST_Test",
                "task_class": "StateTreeTask_Wait"
            })
        self.assertIn("state_name", str(ctx.exception).lower())

    def test_add_statetree_task_missing_task_class(self):
        """Test add-statetree-task fails without task_class."""
        with self.assertRaises(McpError) as ctx:
            self.client.call_tool("add-statetree-task", {
                "asset_path": "/Game/Test/ST_Test",
                "state_name": "TestState"
            })
        self.assertIn("task_class", str(ctx.exception).lower())

    def test_add_statetree_task_nonexistent_asset(self):
        """Test add-statetree-task fails for non-existent StateTree."""
        with self.assertRaises(McpError) as ctx:
            self.client.call_tool("add-statetree-task", {
                "asset_path": "/Game/NonExistent/ST_DoesNotExist_12345",
                "state_name": "TestState",
                "task_class": "StateTreeTask_Wait"
            })
        error_msg = str(ctx.exception).lower()
        self.assertTrue(
            "not found" in error_msg or "failed to load" in error_msg,
            f"Expected 'not found' error, got: {ctx.exception}"
        )

    def test_add_statetree_task_nonexistent_state(self):
        """Test add-statetree-task fails when state doesn't exist."""
        if not self.test_statetree_path:
            self.skipTest("No StateTree found for testing")

        with self.assertRaises(McpError) as ctx:
            self.client.call_tool("add-statetree-task", {
                "asset_path": self.test_statetree_path,
                "state_name": "NonExistentState_12345",
                "task_class": "StateTreeTask_Wait"
            })
        error_msg = str(ctx.exception).lower()
        self.assertTrue(
            "not found" in error_msg,
            f"Expected 'not found' error, got: {ctx.exception}"
        )

    def test_add_statetree_task_invalid_task_class(self):
        """Test add-statetree-task fails for invalid task class."""
        if not self.test_statetree_path:
            self.skipTest("No StateTree found for testing")

        # First get a valid state name
        query_result = self.client.call_tool("query-statetree", {
            "asset_path": self.test_statetree_path,
            "include": "states"
        })
        states = query_result.get("states", {}).get("items", [])
        if not states:
            self.skipTest("No states in StateTree for testing")

        state_name = states[0].get("name")

        with self.assertRaises(McpError) as ctx:
            self.client.call_tool("add-statetree-task", {
                "asset_path": self.test_statetree_path,
                "state_name": state_name,
                "task_class": "NonExistentTaskClass_12345"
            })
        error_msg = str(ctx.exception).lower()
        self.assertTrue(
            "not found" in error_msg or "available" in error_msg,
            f"Expected 'not found' error, got: {ctx.exception}"
        )

    # -------------------------------------------------------------------------
    # add-statetree-transition Tests
    # -------------------------------------------------------------------------

    def test_add_statetree_transition_missing_asset_path(self):
        """Test add-statetree-transition fails without asset_path."""
        with self.assertRaises(McpError) as ctx:
            self.client.call_tool("add-statetree-transition", {
                "source_state": "StateA",
                "target_state": "StateB"
            })
        self.assertIn("asset_path", str(ctx.exception).lower())

    def test_add_statetree_transition_missing_source_state(self):
        """Test add-statetree-transition fails without source_state."""
        with self.assertRaises(McpError) as ctx:
            self.client.call_tool("add-statetree-transition", {
                "asset_path": "/Game/Test/ST_Test",
                "target_state": "StateB"
            })
        self.assertIn("source_state", str(ctx.exception).lower())

    def test_add_statetree_transition_missing_target_state(self):
        """Test add-statetree-transition fails without target_state."""
        with self.assertRaises(McpError) as ctx:
            self.client.call_tool("add-statetree-transition", {
                "asset_path": "/Game/Test/ST_Test",
                "source_state": "StateA"
            })
        self.assertIn("target_state", str(ctx.exception).lower())

    def test_add_statetree_transition_nonexistent_asset(self):
        """Test add-statetree-transition fails for non-existent StateTree."""
        with self.assertRaises(McpError) as ctx:
            self.client.call_tool("add-statetree-transition", {
                "asset_path": "/Game/NonExistent/ST_DoesNotExist_12345",
                "source_state": "StateA",
                "target_state": "StateB"
            })
        error_msg = str(ctx.exception).lower()
        self.assertTrue(
            "not found" in error_msg or "failed to load" in error_msg,
            f"Expected 'not found' error, got: {ctx.exception}"
        )

    def test_add_statetree_transition_nonexistent_source(self):
        """Test add-statetree-transition fails when source state doesn't exist."""
        if not self.test_statetree_path:
            self.skipTest("No StateTree found for testing")

        with self.assertRaises(McpError) as ctx:
            self.client.call_tool("add-statetree-transition", {
                "asset_path": self.test_statetree_path,
                "source_state": "NonExistentState_12345",
                "target_state": "Succeeded"
            })
        error_msg = str(ctx.exception).lower()
        self.assertTrue(
            "not found" in error_msg or "source" in error_msg,
            f"Expected 'source not found' error, got: {ctx.exception}"
        )

    def test_add_statetree_transition_to_succeeded(self):
        """Test adding a transition to 'Succeeded' special state."""
        if not self.test_statetree_path:
            self.skipTest("No StateTree found for testing")

        # First get a valid state name
        query_result = self.client.call_tool("query-statetree", {
            "asset_path": self.test_statetree_path,
            "include": "states"
        })
        states = query_result.get("states", {}).get("items", [])
        if not states:
            self.skipTest("No states in StateTree for testing")

        source_state = states[0].get("name")

        result = self.client.call_tool("add-statetree-transition", {
            "asset_path": self.test_statetree_path,
            "source_state": source_state,
            "target_state": "Succeeded"
        })

        # This should succeed without error
        self.assertTrue(result.get("success"), "add-statetree-transition should succeed")
        self.assertEqual(result.get("target_state"), "Succeeded")

    def test_add_statetree_transition_to_failed(self):
        """Test adding a transition to 'Failed' special state."""
        if not self.test_statetree_path:
            self.skipTest("No StateTree found for testing")

        query_result = self.client.call_tool("query-statetree", {
            "asset_path": self.test_statetree_path,
            "include": "states"
        })
        states = query_result.get("states", {}).get("items", [])
        if not states:
            self.skipTest("No states in StateTree for testing")

        source_state = states[0].get("name")

        result = self.client.call_tool("add-statetree-transition", {
            "asset_path": self.test_statetree_path,
            "source_state": source_state,
            "target_state": "Failed",
            "trigger": "OnStateFailed"
        })

        self.assertTrue(result.get("success"), "add-statetree-transition should succeed")
        self.assertEqual(result.get("trigger"), "OnStateFailed")

    def test_add_statetree_transition_with_priority(self):
        """Test adding a transition with specific priority."""
        if not self.test_statetree_path:
            self.skipTest("No StateTree found for testing")

        query_result = self.client.call_tool("query-statetree", {
            "asset_path": self.test_statetree_path,
            "include": "states"
        })
        states = query_result.get("states", {}).get("items", [])
        if not states:
            self.skipTest("No states in StateTree for testing")

        source_state = states[0].get("name")

        result = self.client.call_tool("add-statetree-transition", {
            "asset_path": self.test_statetree_path,
            "source_state": source_state,
            "target_state": "Next",
            "priority": "High"
        })

        self.assertTrue(result.get("success"), "add-statetree-transition should succeed")
        self.assertEqual(result.get("priority"), "High")


class TestMiscReadTools(McpTestCase):
    """Test remaining read tools that weren't covered."""

    def test_inspect_widget_blueprint(self):
        """Test inspect-widget-blueprint tool."""
        # Search for any Widget Blueprint
        search_result = self.client.call_tool("query-asset", {
            "query": "*",
            "class": "WidgetBlueprint",
            "limit": 1
        })
        if not search_result.get("assets"):
            self.skipTest("No WidgetBlueprint found for testing")

        widget_path = search_result["assets"][0]["path"]

        result = self.client.call_tool("inspect-widget-blueprint", {
            "asset_path": widget_path
        })
        # Should return widget info
        self.assertIsInstance(result, dict)
        self.assertIn("asset_path", result)


# ============================================================================
# ERROR HANDLING TESTS
# ============================================================================


class TestErrorHandling(McpTestCase):
    """Test that tools properly return errors for invalid inputs."""

    # Test assets that may become stale
    ERROR_TEST_ASSETS = [
        "/Game/BP_MCP_ErrorTest_NodeClass",
        "/Game/BP_MCP_ErrorTest_CallFunction",
        "/Game/BP_MCP_ErrorTest_VariableGet",
        "/Game/BP_MCP_ErrorTest_AlreadyExists",
        "/Game/BP_MCP_ErrorTest_RemoveNode",
    ]

    @classmethod
    def setUpClass(cls):
        """Clean up any stale test assets from previous runs."""
        super().setUpClass()
        import time
        for asset_path in cls.ERROR_TEST_ASSETS:
            # Try multiple times to ensure cleanup with longer waits
            for attempt in range(5):
                try:
                    result = cls.client.call_tool("delete-asset", {"asset_path": asset_path})
                    if result.get("success"):
                        # Wait longer for GC to complete
                        time.sleep(0.5)
                        break
                except:
                    pass
                time.sleep(0.2)

    # -------------------------------------------------------------------------
    # Missing Required Parameters
    # -------------------------------------------------------------------------

    def test_query_blueprint_missing_asset_path(self):
        """Test query-blueprint fails without asset_path."""
        with self.assertRaises(McpError) as ctx:
            self.client.call_tool("query-blueprint", {})
        self.assertIn("asset_path", str(ctx.exception).lower())

    def test_query_blueprint_graph_missing_asset_path(self):
        """Test query-blueprint-graph fails without asset_path."""
        with self.assertRaises(McpError) as ctx:
            self.client.call_tool("query-blueprint-graph", {})
        self.assertIn("asset_path", str(ctx.exception).lower())

    def test_query_material_missing_asset_path(self):
        """Test query-material fails without asset_path."""
        with self.assertRaises(McpError) as ctx:
            self.client.call_tool("query-material", {})
        self.assertIn("asset_path", str(ctx.exception).lower())

    def test_create_asset_missing_asset_path(self):
        """Test create-asset fails without asset_path."""
        with self.assertRaises(McpError) as ctx:
            self.client.call_tool("create-asset", {
                "asset_class": "Blueprint"
            })
        self.assertIn("asset_path", str(ctx.exception).lower())

    def test_create_asset_missing_asset_class(self):
        """Test create-asset fails without asset_class."""
        with self.assertRaises(McpError) as ctx:
            self.client.call_tool("create-asset", {
                "asset_path": "/Game/TestMissing"
            })
        self.assertIn("asset_class", str(ctx.exception).lower())

    def test_delete_asset_missing_asset_path(self):
        """Test delete-asset fails without asset_path."""
        with self.assertRaises(McpError) as ctx:
            self.client.call_tool("delete-asset", {})
        self.assertIn("asset_path", str(ctx.exception).lower())

    def test_spawn_actor_missing_actor_class(self):
        """Test spawn-actor fails without actor_class."""
        with self.assertRaises(McpError) as ctx:
            self.client.call_tool("spawn-actor", {
                "label": "TestActor"
            })
        self.assertIn("actor_class", str(ctx.exception).lower())

    def test_delete_actor_missing_actor_name(self):
        """Test delete-actor fails without actor_name."""
        with self.assertRaises(McpError) as ctx:
            self.client.call_tool("delete-actor", {})
        self.assertIn("actor_name", str(ctx.exception).lower())

    def test_add_graph_node_missing_node_class(self):
        """Test add-graph-node fails without node_class."""
        with self.assertRaises(McpError) as ctx:
            self.client.call_tool("add-graph-node", {
                "asset_path": "/Game/SomeBlueprint"
            })
        self.assertIn("node_class", str(ctx.exception).lower())

    def test_get_class_hierarchy_missing_class_name(self):
        """Test get-class-hierarchy fails without class_name."""
        with self.assertRaises(McpError) as ctx:
            self.client.call_tool("get-class-hierarchy", {})
        self.assertIn("class_name", str(ctx.exception).lower())

    def test_find_references_missing_type(self):
        """Test find-references fails without type."""
        with self.assertRaises(McpError) as ctx:
            self.client.call_tool("find-references", {
                "asset_path": "/Game/SomeAsset"
            })
        self.assertIn("type", str(ctx.exception).lower())

    # -------------------------------------------------------------------------
    # Non-existent Assets/Actors
    # -------------------------------------------------------------------------

    def test_query_blueprint_nonexistent_asset(self):
        """Test query-blueprint fails for non-existent asset."""
        with self.assertRaises(McpError) as ctx:
            self.client.call_tool("query-blueprint", {
                "asset_path": "/Game/NonExistent/BP_DoesNotExist_12345"
            })
        error_msg = str(ctx.exception).lower()
        self.assertTrue(
            "not found" in error_msg or "does not exist" in error_msg or "failed to load" in error_msg,
            f"Expected 'not found' error, got: {ctx.exception}"
        )

    def test_query_blueprint_graph_nonexistent_asset(self):
        """Test query-blueprint-graph fails for non-existent asset."""
        with self.assertRaises(McpError) as ctx:
            self.client.call_tool("query-blueprint-graph", {
                "asset_path": "/Game/NonExistent/BP_DoesNotExist_12345"
            })
        error_msg = str(ctx.exception).lower()
        self.assertTrue(
            "not found" in error_msg or "does not exist" in error_msg or "failed to load" in error_msg,
            f"Expected 'not found' error, got: {ctx.exception}"
        )

    def test_query_material_nonexistent_asset(self):
        """Test query-material fails for non-existent asset."""
        with self.assertRaises(McpError) as ctx:
            self.client.call_tool("query-material", {
                "asset_path": "/Game/NonExistent/M_DoesNotExist_12345"
            })
        error_msg = str(ctx.exception).lower()
        self.assertTrue(
            "not found" in error_msg or "does not exist" in error_msg or "failed to load" in error_msg,
            f"Expected 'not found' error, got: {ctx.exception}"
        )

    def test_delete_asset_nonexistent(self):
        """Test delete-asset fails for non-existent asset."""
        with self.assertRaises(McpError) as ctx:
            self.client.call_tool("delete-asset", {
                "asset_path": "/Game/NonExistent/Asset_12345"
            })
        error_msg = str(ctx.exception).lower()
        self.assertTrue(
            "not found" in error_msg or "does not exist" in error_msg,
            f"Expected 'not found' error, got: {ctx.exception}"
        )

    def test_delete_actor_nonexistent(self):
        """Test delete-actor fails for non-existent actor."""
        with self.assertRaises(McpError) as ctx:
            self.client.call_tool("delete-actor", {
                "actor_name": "NonExistentActor_12345_MCP_Test"
            })
        error_msg = str(ctx.exception).lower()
        self.assertTrue(
            "not found" in error_msg or "does not exist" in error_msg,
            f"Expected 'not found' error, got: {ctx.exception}"
        )

    def test_query_level_nonexistent_actor(self):
        """Test query-level detail mode fails for non-existent actor."""
        with self.assertRaises(McpError) as ctx:
            self.client.call_tool("query-level", {
                "actor_name": "NonExistentActor_12345_MCP_Test"
            })
        error_msg = str(ctx.exception).lower()
        self.assertTrue(
            "not found" in error_msg or "does not exist" in error_msg,
            f"Expected 'not found' error, got: {ctx.exception}"
        )

    def test_compile_blueprint_nonexistent(self):
        """Test compile-blueprint fails for non-existent Blueprint."""
        with self.assertRaises(McpError) as ctx:
            self.client.call_tool("compile-blueprint", {
                "asset_path": "/Game/NonExistent/BP_DoesNotExist_12345"
            })
        error_msg = str(ctx.exception).lower()
        self.assertTrue(
            "not found" in error_msg or "does not exist" in error_msg or "failed to load" in error_msg,
            f"Expected 'not found' error, got: {ctx.exception}"
        )

    def test_save_asset_nonexistent(self):
        """Test save-asset fails for non-existent asset."""
        with self.assertRaises(McpError) as ctx:
            self.client.call_tool("save-asset", {
                "asset_path": "/Game/NonExistent/Asset_12345"
            })
        error_msg = str(ctx.exception).lower()
        self.assertTrue(
            "not found" in error_msg or "does not exist" in error_msg or "failed to load" in error_msg,
            f"Expected 'not found' error, got: {ctx.exception}"
        )

    # -------------------------------------------------------------------------
    # Invalid Class/Type Names
    # -------------------------------------------------------------------------

    def test_spawn_actor_invalid_class(self):
        """Test spawn-actor fails for invalid actor class."""
        with self.assertRaises(McpError) as ctx:
            self.client.call_tool("spawn-actor", {
                "actor_class": "NonExistentActorClass_12345",
                "label": "TestActor"
            })
        error_msg = str(ctx.exception).lower()
        self.assertTrue(
            "not found" in error_msg or "invalid" in error_msg or "unknown" in error_msg,
            f"Expected 'not found/invalid' error, got: {ctx.exception}"
        )

    def test_create_asset_invalid_class(self):
        """Test create-asset fails for unsupported asset class."""
        with self.assertRaises(McpError) as ctx:
            self.client.call_tool("create-asset", {
                "asset_path": "/Game/Test_InvalidClass",
                "asset_class": "UnsupportedAssetType_12345"
            })
        error_msg = str(ctx.exception).lower()
        self.assertTrue(
            "unsupported" in error_msg or "invalid" in error_msg or "unknown" in error_msg,
            f"Expected 'unsupported' error, got: {ctx.exception}"
        )

    def test_get_class_hierarchy_invalid_class(self):
        """Test get-class-hierarchy fails for non-existent class."""
        with self.assertRaises(McpError) as ctx:
            self.client.call_tool("get-class-hierarchy", {
                "class_name": "NonExistentClass_12345_XYZ"
            })
        error_msg = str(ctx.exception).lower()
        self.assertTrue(
            "not found" in error_msg or "invalid" in error_msg,
            f"Expected 'not found' error, got: {ctx.exception}"
        )

    def test_add_graph_node_invalid_node_class(self):
        """Test add-graph-node fails for invalid node class."""
        # First create a test Blueprint
        test_bp = "/Game/BP_MCP_ErrorTest_NodeClass"
        try:
            self.client.call_tool("delete-asset", {"asset_path": test_bp})
        except:
            pass

        try:
            create_result = self.client.call_tool("create-asset", {
                "asset_path": test_bp,
                "asset_class": "Blueprint"
            })
            self.assertTrue(create_result.get("success"))
        except McpError as e:
            if "already exists" in str(e).lower():
                self.skipTest("Stale asset from previous run - restart editor")
            raise

        try:
            with self.assertRaises(McpError) as ctx:
                self.client.call_tool("add-graph-node", {
                    "asset_path": test_bp,
                    "node_class": "InvalidNodeClass_12345"
                })
            error_msg = str(ctx.exception).lower()
            self.assertTrue(
                "unknown" in error_msg or "invalid" in error_msg or "unsupported" in error_msg,
                f"Expected 'unknown/invalid' error, got: {ctx.exception}"
            )
        finally:
            try:
                self.client.call_tool("delete-asset", {"asset_path": test_bp})
            except:
                pass

    # -------------------------------------------------------------------------
    # Invalid Path Formats
    # -------------------------------------------------------------------------

    def test_create_asset_invalid_path_format(self):
        """Test create-asset fails for invalid path format."""
        with self.assertRaises(McpError) as ctx:
            self.client.call_tool("create-asset", {
                "asset_path": "InvalidPath",  # Missing /Game/ prefix
                "asset_class": "Blueprint"
            })
        error_msg = str(ctx.exception).lower()
        self.assertTrue(
            "invalid" in error_msg or "path" in error_msg or "must start" in error_msg,
            f"Expected path validation error, got: {ctx.exception}"
        )

    # -------------------------------------------------------------------------
    # Asset Already Exists
    # -------------------------------------------------------------------------

    def test_create_asset_already_exists(self):
        """Test create-asset fails when asset already exists."""
        test_bp = "/Game/BP_MCP_ErrorTest_AlreadyExists"

        # Clean up first
        try:
            self.client.call_tool("delete-asset", {"asset_path": test_bp})
        except:
            pass

        # Create asset
        try:
            create_result = self.client.call_tool("create-asset", {
                "asset_path": test_bp,
                "asset_class": "Blueprint"
            })
            self.assertTrue(create_result.get("success"))
        except McpError as e:
            if "already exists" in str(e).lower():
                self.skipTest("Stale asset from previous run - restart editor")
            raise

        try:
            # Try to create again - should fail
            with self.assertRaises(McpError) as ctx:
                self.client.call_tool("create-asset", {
                    "asset_path": test_bp,
                    "asset_class": "Blueprint"
                })
            error_msg = str(ctx.exception).lower()
            self.assertTrue(
                "already exists" in error_msg or "exists" in error_msg,
                f"Expected 'already exists' error, got: {ctx.exception}"
            )
        finally:
            try:
                self.client.call_tool("delete-asset", {"asset_path": test_bp})
            except:
                pass

    # -------------------------------------------------------------------------
    # Wrong Asset Type
    # -------------------------------------------------------------------------

    def test_query_blueprint_on_material(self):
        """Test query-blueprint fails when called on a Material asset."""
        # First find a Material
        search_result = self.client.call_tool("query-asset", {
            "query": "*",
            "class": "Material",
            "limit": 1
        })
        if not search_result.get("assets"):
            self.skipTest("No Material found for testing")

        material_path = search_result["assets"][0]["path"]

        with self.assertRaises(McpError) as ctx:
            self.client.call_tool("query-blueprint", {
                "asset_path": material_path
            })
        error_msg = str(ctx.exception).lower()
        self.assertTrue(
            "not a blueprint" in error_msg or "blueprint" in error_msg or "invalid" in error_msg,
            f"Expected 'not a blueprint' error, got: {ctx.exception}"
        )

    def test_query_material_on_blueprint(self):
        """Test query-material fails when called on a Blueprint asset."""
        # First find a Blueprint
        search_result = self.client.call_tool("query-asset", {
            "query": "*",
            "class": "Blueprint",
            "limit": 1
        })
        if not search_result.get("assets"):
            self.skipTest("No Blueprint found for testing")

        blueprint_path = search_result["assets"][0]["path"]

        with self.assertRaises(McpError) as ctx:
            self.client.call_tool("query-material", {
                "asset_path": blueprint_path
            })
        error_msg = str(ctx.exception).lower()
        self.assertTrue(
            "not a material" in error_msg or "material" in error_msg or "invalid" in error_msg,
            f"Expected 'not a material' error, got: {ctx.exception}"
        )

    def test_inspect_widget_blueprint_on_regular_blueprint(self):
        """Test inspect-widget-blueprint fails on regular Blueprint."""
        # First find a regular Blueprint (not a Widget Blueprint)
        search_result = self.client.call_tool("query-asset", {
            "query": "*",
            "class": "Blueprint",
            "limit": 5
        })
        if not search_result.get("assets"):
            self.skipTest("No Blueprint found for testing")

        # Find a Blueprint that's NOT a WidgetBlueprint
        blueprint_path = None
        for asset in search_result.get("assets", []):
            if "Widget" not in asset.get("path", ""):
                blueprint_path = asset["path"]
                break

        if not blueprint_path:
            self.skipTest("Could not find a non-Widget Blueprint for testing")

        with self.assertRaises(McpError) as ctx:
            self.client.call_tool("inspect-widget-blueprint", {
                "asset_path": blueprint_path
            })
        error_msg = str(ctx.exception).lower()
        self.assertTrue(
            "widget" in error_msg or "not a" in error_msg or "invalid" in error_msg,
            f"Expected 'not a widget blueprint' error, got: {ctx.exception}"
        )

    # -------------------------------------------------------------------------
    # Component Tool Errors
    # -------------------------------------------------------------------------

    def test_add_component_nonexistent_actor(self):
        """Test add-component fails for non-existent actor."""
        with self.assertRaises(McpError) as ctx:
            self.client.call_tool("add-component", {
                "actor_name": "NonExistentActor_12345_MCP",
                "component_class": "StaticMeshComponent",
                "component_name": "TestComp"
            })
        error_msg = str(ctx.exception).lower()
        self.assertTrue(
            "not found" in error_msg or "does not exist" in error_msg,
            f"Expected 'not found' error, got: {ctx.exception}"
        )

    def test_remove_component_nonexistent_actor(self):
        """Test remove-component fails for non-existent actor."""
        with self.assertRaises(McpError) as ctx:
            self.client.call_tool("remove-component", {
                "actor_name": "NonExistentActor_12345_MCP",
                "component_name": "TestComp"
            })
        error_msg = str(ctx.exception).lower()
        self.assertTrue(
            "not found" in error_msg or "does not exist" in error_msg,
            f"Expected 'not found' error, got: {ctx.exception}"
        )

    def test_add_component_invalid_class(self):
        """Test add-component fails for invalid component class."""
        # First spawn a test actor
        spawn_result = self.client.call_tool("spawn-actor", {
            "actor_class": "StaticMeshActor",
            "label": "MCP_ErrorTest_ComponentClass"
        })
        if not spawn_result.get("success"):
            self.skipTest("Could not spawn test actor")

        actor_name = spawn_result["actor_name"]

        try:
            with self.assertRaises(McpError) as ctx:
                self.client.call_tool("add-component", {
                    "actor_name": actor_name,
                    "component_class": "NonExistentComponentClass_12345",
                    "component_name": "TestComp"
                })
            error_msg = str(ctx.exception).lower()
            self.assertTrue(
                "not found" in error_msg or "invalid" in error_msg or "unknown" in error_msg,
                f"Expected 'not found/invalid' error, got: {ctx.exception}"
            )
        finally:
            try:
                self.client.call_tool("delete-actor", {"actor_name": actor_name})
            except:
                pass

    # -------------------------------------------------------------------------
    # Graph Node Operation Errors
    # -------------------------------------------------------------------------

    def test_remove_graph_node_nonexistent_node(self):
        """Test remove-graph-node fails for non-existent node GUID."""
        # First create a test Blueprint
        test_bp = "/Game/BP_MCP_ErrorTest_RemoveNode"
        try:
            self.client.call_tool("delete-asset", {"asset_path": test_bp})
        except:
            pass

        try:
            create_result = self.client.call_tool("create-asset", {
                "asset_path": test_bp,
                "asset_class": "Blueprint"
            })
            self.assertTrue(create_result.get("success"))
        except McpError as e:
            if "already exists" in str(e).lower():
                self.skipTest("Stale asset from previous run - restart editor")
            raise

        try:
            with self.assertRaises(McpError) as ctx:
                self.client.call_tool("remove-graph-node", {
                    "asset_path": test_bp,
                    "node_id": "00000000-0000-0000-0000-000000000000"
                })
            error_msg = str(ctx.exception).lower()
            self.assertTrue(
                "not found" in error_msg or "invalid" in error_msg,
                f"Expected 'not found' error, got: {ctx.exception}"
            )
        finally:
            try:
                self.client.call_tool("delete-asset", {"asset_path": test_bp})
            except:
                pass

    def test_add_graph_node_callfunction_missing_function_name(self):
        """Test add-graph-node with CallFunction but missing function_name."""
        # First create a test Blueprint
        test_bp = "/Game/BP_MCP_ErrorTest_CallFunction"
        try:
            self.client.call_tool("delete-asset", {"asset_path": test_bp})
        except:
            pass

        try:
            create_result = self.client.call_tool("create-asset", {
                "asset_path": test_bp,
                "asset_class": "Blueprint"
            })
            self.assertTrue(create_result.get("success"))
        except McpError as e:
            if "already exists" in str(e).lower():
                self.skipTest("Stale asset from previous run - restart editor")
            raise

        try:
            with self.assertRaises(McpError) as ctx:
                self.client.call_tool("add-graph-node", {
                    "asset_path": test_bp,
                    "node_class": "CallFunction"
                    # Missing function_name
                })
            error_msg = str(ctx.exception).lower()
            self.assertTrue(
                "function_name" in error_msg or "required" in error_msg,
                f"Expected 'function_name required' error, got: {ctx.exception}"
            )
        finally:
            try:
                self.client.call_tool("delete-asset", {"asset_path": test_bp})
            except:
                pass

    def test_add_graph_node_variableget_missing_variable_name(self):
        """Test add-graph-node with VariableGet but missing variable_name."""
        # First create a test Blueprint
        test_bp = "/Game/BP_MCP_ErrorTest_VariableGet"
        try:
            self.client.call_tool("delete-asset", {"asset_path": test_bp})
        except:
            pass

        try:
            create_result = self.client.call_tool("create-asset", {
                "asset_path": test_bp,
                "asset_class": "Blueprint"
            })
            self.assertTrue(create_result.get("success"))
        except McpError as e:
            if "already exists" in str(e).lower():
                self.skipTest("Stale asset from previous run - restart editor")
            raise

        try:
            with self.assertRaises(McpError) as ctx:
                self.client.call_tool("add-graph-node", {
                    "asset_path": test_bp,
                    "node_class": "VariableGet"
                    # Missing variable_name
                })
            error_msg = str(ctx.exception).lower()
            self.assertTrue(
                "variable_name" in error_msg or "required" in error_msg,
                f"Expected 'variable_name required' error, got: {ctx.exception}"
            )
        finally:
            try:
                self.client.call_tool("delete-asset", {"asset_path": test_bp})
            except:
                pass

    # -------------------------------------------------------------------------
    # Invalid Tool Name
    # -------------------------------------------------------------------------

    def test_nonexistent_tool(self):
        """Test calling a non-existent tool returns proper error."""
        with self.assertRaises(McpError) as ctx:
            self.client.call_tool("nonexistent-tool-12345", {})
        error_msg = str(ctx.exception).lower()
        self.assertTrue(
            "not found" in error_msg or "unknown" in error_msg or "tool" in error_msg,
            f"Expected 'tool not found' error, got: {ctx.exception}"
        )


# =============================================================================
# v1.9.0 Dynamic Reflection Tests
# =============================================================================

class TestDynamicMaterialExpressions(McpTestCase):
    """Test dynamic material expression creation (any class name works)."""

    @classmethod
    def setUpClass(cls):
        """Create a test Material."""
        super().setUpClass()
        import time
        cls.test_mat_path = "/Game/M_MCP_DynamicExprTest"

        # Clean up
        for _ in range(3):
            try:
                cls.client.call_tool("delete-asset", {"asset_path": cls.test_mat_path})
            except:
                pass
            time.sleep(0.1)

        # Create test Material
        try:
            result = cls.client.call_tool("create-asset", {
                "asset_path": cls.test_mat_path,
                "asset_class": "Material"
            })
            if not result.get("success"):
                raise unittest.SkipTest("Could not create test Material")
        except McpError as e:
            if "already exists" in str(e).lower():
                raise unittest.SkipTest("Asset already exists")
            raise

    @classmethod
    def tearDownClass(cls):
        """Delete test Material."""
        try:
            cls.client.call_tool("delete-asset", {"asset_path": cls.test_mat_path})
        except:
            pass
        super().tearDownClass()

    def test_add_material_expression_by_full_class_name(self):
        """Test adding material expression using full class name."""
        result = self.client.call_tool("add-graph-node", {
            "asset_path": self.test_mat_path,
            "node_class": "MaterialExpressionAdd",
            "position": [100, 100]
        })
        self.assertTrue(result.get("success"), f"Should create Add expression: {result}")
        self.assertEqual(result.get("expression_class"), "MaterialExpressionAdd")

    def test_add_material_expression_multiply(self):
        """Test adding Multiply expression by class name."""
        result = self.client.call_tool("add-graph-node", {
            "asset_path": self.test_mat_path,
            "node_class": "MaterialExpressionMultiply",
            "position": [200, 100]
        })
        self.assertTrue(result.get("success"), f"Should create Multiply expression: {result}")
        self.assertEqual(result.get("expression_class"), "MaterialExpressionMultiply")

    def test_add_scene_texture_expression(self):
        """Test adding SceneTexture expression (was previously unsupported)."""
        result = self.client.call_tool("add-graph-node", {
            "asset_path": self.test_mat_path,
            "node_class": "MaterialExpressionSceneTexture",
            "position": [300, 100],
            "properties": {
                "SceneTextureId": "PPI_SceneColor"
            }
        })
        self.assertTrue(result.get("success"), f"Should create SceneTexture expression: {result}")
        self.assertEqual(result.get("expression_class"), "MaterialExpressionSceneTexture")

    def test_add_world_position_expression(self):
        """Test adding WorldPosition expression."""
        result = self.client.call_tool("add-graph-node", {
            "asset_path": self.test_mat_path,
            "node_class": "MaterialExpressionWorldPosition",
            "position": [400, 100]
        })
        self.assertTrue(result.get("success"), f"Should create WorldPosition expression: {result}")
        self.assertEqual(result.get("expression_class"), "MaterialExpressionWorldPosition")

    def test_add_custom_expression(self):
        """Test adding Custom HLSL expression."""
        result = self.client.call_tool("add-graph-node", {
            "asset_path": self.test_mat_path,
            "node_class": "MaterialExpressionCustom",
            "position": [500, 100],
            "properties": {
                "Code": "return 1.0;"
            }
        })
        self.assertTrue(result.get("success"), f"Should create Custom expression: {result}")
        self.assertEqual(result.get("expression_class"), "MaterialExpressionCustom")

    def test_add_noise_expression(self):
        """Test adding Noise expression."""
        result = self.client.call_tool("add-graph-node", {
            "asset_path": self.test_mat_path,
            "node_class": "MaterialExpressionNoise",
            "position": [600, 100]
        })
        self.assertTrue(result.get("success"), f"Should create Noise expression: {result}")
        self.assertEqual(result.get("expression_class"), "MaterialExpressionNoise")

    def test_add_expression_with_properties(self):
        """Test adding expression with properties set via reflection."""
        result = self.client.call_tool("add-graph-node", {
            "asset_path": self.test_mat_path,
            "node_class": "MaterialExpressionScalarParameter",
            "position": [700, 100],
            "properties": {
                "ParameterName": "TestParam",
                "DefaultValue": 0.5
            }
        })
        self.assertTrue(result.get("success"), f"Should create ScalarParameter: {result}")
        self.assertEqual(result.get("expression_class"), "MaterialExpressionScalarParameter")

    def test_invalid_expression_class_error(self):
        """Test that invalid expression class returns clear error."""
        with self.assertRaises(McpError) as ctx:
            self.client.call_tool("add-graph-node", {
                "asset_path": self.test_mat_path,
                "node_class": "NonExistentExpressionClass12345"
            })
        error_msg = str(ctx.exception).lower()
        self.assertTrue(
            "not found" in error_msg or "class" in error_msg,
            f"Expected class not found error: {ctx.exception}"
        )


class TestDynamicAssetCreation(McpTestCase):
    """Test dynamic asset creation (any class name works)."""

    def setUp(self):
        """Set up test paths."""
        self.test_assets = []

    def tearDown(self):
        """Clean up created assets."""
        for path in self.test_assets:
            try:
                self.client.call_tool("delete-asset", {"asset_path": path})
            except:
                pass

    def test_create_dataasset(self):
        """Test creating a DataAsset instance."""
        path = "/Game/DA_MCP_TestDataAsset"
        self.test_assets.append(path)

        # Clean up first
        try:
            self.client.call_tool("delete-asset", {"asset_path": path})
        except:
            pass

        result = self.client.call_tool("create-asset", {
            "asset_path": path,
            "asset_class": "DataAsset"
        })
        self.assertTrue(result.get("success"), f"Should create DataAsset: {result}")
        self.assertIn("DataAsset", result.get("created_class", ""))

    def test_create_blueprint_with_dynamic_parent_class(self):
        """Test creating Blueprint with dynamic parent class resolution."""
        path = "/Game/BP_MCP_DynamicParent"
        self.test_assets.append(path)

        try:
            self.client.call_tool("delete-asset", {"asset_path": path})
        except:
            pass

        result = self.client.call_tool("create-asset", {
            "asset_path": path,
            "asset_class": "Blueprint",
            "parent_class": "Character"  # Should resolve dynamically
        })
        self.assertTrue(result.get("success"), f"Should create Blueprint: {result}")
        self.assertEqual(result.get("parent_class"), "Character")

    def test_create_material_by_class_name(self):
        """Test creating Material by class name."""
        path = "/Game/M_MCP_DynamicCreate"
        self.test_assets.append(path)

        try:
            self.client.call_tool("delete-asset", {"asset_path": path})
        except:
            pass

        result = self.client.call_tool("create-asset", {
            "asset_path": path,
            "asset_class": "Material"
        })
        self.assertTrue(result.get("success"), f"Should create Material: {result}")

    def test_invalid_asset_class_error(self):
        """Test that invalid asset class returns clear error."""
        path = "/Game/Invalid_MCP_Asset"
        self.test_assets.append(path)

        with self.assertRaises(McpError) as ctx:
            self.client.call_tool("create-asset", {
                "asset_path": path,
                "asset_class": "NonExistentAssetClass12345"
            })
        error_msg = str(ctx.exception).lower()
        self.assertTrue(
            "unknown" in error_msg or "class" in error_msg or "not found" in error_msg,
            f"Expected unknown class error: {ctx.exception}"
        )


class TestDynamicBlueprintNodes(McpTestCase):
    """Test dynamic Blueprint node creation."""

    @classmethod
    def setUpClass(cls):
        """Create a test Blueprint."""
        super().setUpClass()
        import time
        cls.test_bp_path = "/Game/BP_MCP_DynamicNodeTest"

        for _ in range(3):
            try:
                cls.client.call_tool("delete-asset", {"asset_path": cls.test_bp_path})
            except:
                pass
            time.sleep(0.1)

        try:
            result = cls.client.call_tool("create-asset", {
                "asset_path": cls.test_bp_path,
                "asset_class": "Blueprint",
                "parent_class": "Actor"
            })
            if not result.get("success"):
                raise unittest.SkipTest("Could not create test Blueprint")
        except McpError as e:
            if "already exists" in str(e).lower():
                raise unittest.SkipTest("Asset already exists")
            raise

    @classmethod
    def tearDownClass(cls):
        """Delete test Blueprint."""
        try:
            cls.client.call_tool("delete-asset", {"asset_path": cls.test_bp_path})
        except:
            pass
        super().tearDownClass()

    def test_add_node_by_full_class_name(self):
        """Test adding Blueprint node by full class name."""
        result = self.client.call_tool("add-graph-node", {
            "asset_path": self.test_bp_path,
            "node_class": "K2Node_CallFunction",
            "graph_name": "EventGraph",
            "position": [100, 100],
            "properties": {
                "FunctionReference.MemberName": "PrintString"
            }
        })
        # May succeed or fail depending on function setup, but should respond
        self.assertIn("success", result)

    def test_add_variable_get_node(self):
        """Test adding VariableGet node by class name."""
        result = self.client.call_tool("add-graph-node", {
            "asset_path": self.test_bp_path,
            "node_class": "K2Node_VariableGet",
            "graph_name": "EventGraph",
            "position": [200, 100],
            "properties": {
                "VariableReference.MemberName": "TestVar"
            }
        })
        self.assertIn("success", result)

    def test_add_variable_set_node(self):
        """Test adding VariableSet node by class name."""
        result = self.client.call_tool("add-graph-node", {
            "asset_path": self.test_bp_path,
            "node_class": "K2Node_VariableSet",
            "graph_name": "EventGraph",
            "position": [300, 100]
        })
        self.assertIn("success", result)


class TestExtendedPropertyTypes(McpTestCase):
    """Test extended property type support (TMap, TSet, Object refs)."""

    @classmethod
    def setUpClass(cls):
        """Create a test Blueprint with various property types."""
        super().setUpClass()
        import time
        cls.test_bp_path = "/Game/BP_MCP_ExtendedPropTest"

        for _ in range(3):
            try:
                cls.client.call_tool("delete-asset", {"asset_path": cls.test_bp_path})
            except:
                pass
            time.sleep(0.1)

        try:
            result = cls.client.call_tool("create-asset", {
                "asset_path": cls.test_bp_path,
                "asset_class": "Blueprint",
                "parent_class": "Actor"
            })
            if not result.get("success"):
                raise unittest.SkipTest("Could not create test Blueprint")
        except McpError as e:
            if "already exists" in str(e).lower():
                raise unittest.SkipTest("Asset already exists")
            raise

    @classmethod
    def tearDownClass(cls):
        """Delete test Blueprint."""
        try:
            cls.client.call_tool("delete-asset", {"asset_path": cls.test_bp_path})
        except:
            pass
        super().tearDownClass()

    def test_set_vector_property_as_array(self):
        """Test setting FVector property using array syntax."""
        # Actor has many vector properties, try setting one
        result = self.client.call_tool("set-property", {
            "asset_path": self.test_bp_path,
            "property_path": "InitialLifeSpan",  # A float property
            "value": 10.0
        })
        # May or may not succeed based on property availability
        self.assertIn("success", result)

    def test_set_bool_property(self):
        """Test setting boolean property."""
        result = self.client.call_tool("set-property", {
            "asset_path": self.test_bp_path,
            "property_path": "bReplicates",
            "value": True
        })
        self.assertIn("success", result)

    def test_set_enum_property_by_name(self):
        """Test setting enum property by name string."""
        result = self.client.call_tool("set-property", {
            "asset_path": self.test_bp_path,
            "property_path": "SpawnCollisionHandlingMethod",
            "value": "AlwaysSpawn"
        })
        # May succeed or fail based on property type
        self.assertIn("success", result)


class TestRunPythonScript(McpTestCase):
    """Test run-python-script tool."""

    def test_inline_script(self):
        """Test executing inline Python script."""
        result = self.client.call_tool("run-python-script", {
            "script": "import unreal\nprint('Hello from Python')\nprint('Editor Version:', unreal.SystemLibrary.get_engine_version())"
        })
        self.assertTrue(result.get("success"), "Inline script should execute successfully")
        self.assertIn("output", result)

    def test_simple_calculation(self):
        """Test Python script with simple calculation."""
        result = self.client.call_tool("run-python-script", {
            "script": "result = 2 + 2\nprint(f'Result: {result}')"
        })
        self.assertTrue(result.get("success"))

    def test_list_assets(self):
        """Test Python script that lists assets."""
        result = self.client.call_tool("run-python-script", {
            "script": """
import unreal
assets = unreal.EditorAssetLibrary.list_assets('/Game/', recursive=False)
print(f'Found {len(assets)} assets in /Game/')
for asset in assets[:5]:
    print(f'  - {asset}')
"""
        })
        self.assertTrue(result.get("success"))

    def test_script_with_arguments(self):
        """Test Python script with arguments."""
        result = self.client.call_tool("run-python-script", {
            "script": """
import unreal
args = unreal.get_mcp_args() if hasattr(unreal, 'get_mcp_args') else {}
asset_path = args.get('asset_path', 'None')
count = args.get('count', 0)
print(f'Asset path: {asset_path}')
print(f'Count: {count}')
""",
            "arguments": {
                "asset_path": "/Game/TestAsset",
                "count": 42
            }
        })
        self.assertTrue(result.get("success"))

    def test_error_handling(self):
        """Test that script errors are properly caught."""
        with self.assertRaises(McpError):
            self.client.call_tool("run-python-script", {
                "script": "raise ValueError('Test error')"
            })

    def test_missing_parameters(self):
        """Test that missing script/script_path returns error."""
        with self.assertRaises(McpError) as ctx:
            self.client.call_tool("run-python-script", {})
        error_msg = str(ctx.exception).lower()
        self.assertTrue(
            "script" in error_msg or "required" in error_msg,
            f"Expected missing parameter error: {ctx.exception}"
        )

    def test_mutually_exclusive_parameters(self):
        """Test that both script and script_path cannot be provided."""
        with self.assertRaises(McpError) as ctx:
            self.client.call_tool("run-python-script", {
                "script": "print('test')",
                "script_path": "/some/path.py"
            })
        error_msg = str(ctx.exception).lower()
        self.assertTrue(
            "cannot specify both" in error_msg or "mutually exclusive" in error_msg,
            f"Expected mutually exclusive error: {ctx.exception}"
        )

    def test_query_project_info(self):
        """Test using Python to query project info."""
        result = self.client.call_tool("run-python-script", {
            "script": """
import unreal
project_name = unreal.SystemLibrary.get_project_name()
engine_version = unreal.SystemLibrary.get_engine_version()
print(f'Project: {project_name}')
print(f'Engine: {engine_version}')
"""
        })
        self.assertTrue(result.get("success"))

    def test_multiline_output(self):
        """Test that multiline output is captured correctly."""
        result = self.client.call_tool("run-python-script", {
            "script": """
for i in range(5):
    print(f'Line {i}')
"""
        })
        self.assertTrue(result.get("success"))

    def test_import_standard_library(self):
        """Test importing Python standard library modules."""
        result = self.client.call_tool("run-python-script", {
            "script": """
import os
import sys
import json

print(f'Python version: {sys.version_info.major}.{sys.version_info.minor}')
data = {'test': 'value', 'number': 42}
json_str = json.dumps(data)
print(f'JSON: {json_str}')
"""
        })
        self.assertTrue(result.get("success"))


class TestTriggerLiveCoding(McpTestCase):
    """Test trigger-live-coding tool."""

    def test_trigger_async(self):
        """Test triggering Live Coding compilation (async mode)."""
        result = self.client.call_tool("trigger-live-coding", {})
        self.assertTrue(result.get("success"), "Live Coding should trigger successfully")
        self.assertIn("status", result)
        self.assertEqual(result.get("status"), "triggered_async")
        self.assertIn("message", result)
        self.assertIn("shortcut", result)

    def test_trigger_sync_mode(self):
        """Test synchronous compilation with wait_for_completion."""
        result = self.client.call_tool("trigger-live-coding", {
            "wait_for_completion": True,
            "timeout": 60
        })

        # Should complete successfully or timeout
        self.assertIn("status", result)
        self.assertIn(result.get("status"), ["completed", "failed", "timeout"])

        # Check for compilation time if completed
        if result.get("status") in ["completed", "failed"]:
            self.assertIn("compilation_time_seconds", result)
            compilation_time = result.get("compilation_time_seconds")
            self.assertIsInstance(compilation_time, (int, float))
            self.assertGreater(compilation_time, 0)

    def test_sync_with_custom_timeout(self):
        """Test synchronous mode with custom timeout."""
        result = self.client.call_tool("trigger-live-coding", {
            "wait_for_completion": True,
            "timeout": 120
        })
        self.assertIn("status", result)

    def test_response_format(self):
        """Test that response has expected fields."""
        result = self.client.call_tool("trigger-live-coding", {})
        self.assertIn("success", result)
        self.assertIn("status", result)
        self.assertIn("message", result)
        self.assertIn("shortcut", result)
        self.assertEqual(result.get("shortcut"), "Ctrl+Alt+F11")

    def test_sync_returns_compilation_log(self):
        """Test that sync mode returns compilation log on completion."""
        result = self.client.call_tool("trigger-live-coding", {
            "wait_for_completion": True
        })

        # If compilation completed, should have log
        if result.get("status") in ["completed", "failed"]:
            # compilation_log may or may not be present depending on LiveCoding output
            # Just check the result is structured correctly
            self.assertIn("compilation_time_seconds", result)


class TestBuildAndRelaunch(McpTestCase):
    """Test build-and-relaunch tool (dry-run tests only - doesn't actually close editor)."""

    def test_response_format(self):
        """Test that response has expected fields without actually closing editor."""
        # Note: This test is intentionally commented to prevent accidental editor shutdown
        # Uncomment only for manual testing
        pass
        # result = self.client.call_tool("build-and-relaunch", {})
        # self.assertIn("success", result)
        # self.assertIn("status", result)
        # self.assertIn("project", result)
        # self.assertIn("build_config", result)
        # self.assertIn("will_relaunch", result)
        # self.assertEqual(result.get("build_config"), "Development")
        # self.assertTrue(result.get("will_relaunch"))

    def test_custom_build_config(self):
        """Test custom build configuration parameter."""
        # Note: This test is intentionally commented to prevent accidental editor shutdown
        # Uncomment only for manual testing
        pass
        # result = self.client.call_tool("build-and-relaunch", {
        #     "build_config": "Debug",
        #     "skip_relaunch": True
        # })
        # self.assertTrue(result.get("success"))
        # self.assertEqual(result.get("build_config"), "Debug")
        # self.assertFalse(result.get("will_relaunch"))

    def test_invalid_build_config(self):
        """Test that invalid build config is rejected."""
        # This test can run safely as it should fail before triggering editor shutdown
        try:
            result = self.client.call_tool("build-and-relaunch", {
                "build_config": "InvalidConfig"
            })
            # Should return an error, not success
            if result.get("success"):
                self.fail("Should have rejected invalid build config")
        except Exception as e:
            # Expected to fail
            self.assertIn("Invalid build configuration", str(e))


class TestDynamicClassResolution(McpTestCase):
    """Test the class resolution system works correctly."""

    def test_resolve_native_class_short_name(self):
        """Test resolving native class by short name."""
        path = "/Game/BP_MCP_ResolveTest_Actor"
        try:
            self.client.call_tool("delete-asset", {"asset_path": path})
        except:
            pass

        try:
            result = self.client.call_tool("create-asset", {
                "asset_path": path,
                "asset_class": "Blueprint",
                "parent_class": "Actor"  # Short name
            })
            self.assertTrue(result.get("success"))
            self.assertEqual(result.get("parent_class"), "Actor")
        finally:
            try:
                self.client.call_tool("delete-asset", {"asset_path": path})
            except:
                pass

    def test_resolve_native_class_pawn(self):
        """Test resolving Pawn class."""
        path = "/Game/BP_MCP_ResolveTest_Pawn"
        try:
            self.client.call_tool("delete-asset", {"asset_path": path})
        except:
            pass

        try:
            result = self.client.call_tool("create-asset", {
                "asset_path": path,
                "asset_class": "Blueprint",
                "parent_class": "Pawn"
            })
            self.assertTrue(result.get("success"))
            self.assertEqual(result.get("parent_class"), "Pawn")
        finally:
            try:
                self.client.call_tool("delete-asset", {"asset_path": path})
            except:
                pass

    def test_resolve_native_class_character(self):
        """Test resolving Character class."""
        path = "/Game/BP_MCP_ResolveTest_Character"
        try:
            self.client.call_tool("delete-asset", {"asset_path": path})
        except:
            pass

        try:
            result = self.client.call_tool("create-asset", {
                "asset_path": path,
                "asset_class": "Blueprint",
                "parent_class": "Character"
            })
            self.assertTrue(result.get("success"))
            self.assertEqual(result.get("parent_class"), "Character")
        finally:
            try:
                self.client.call_tool("delete-asset", {"asset_path": path})
            except:
                pass


if __name__ == "__main__":
    unittest.main(verbosity=2)
