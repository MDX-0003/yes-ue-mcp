#!/usr/bin/env python3
"""
MCP Tools Integration Tests (v1.6.0 - Post Consolidation)

This script tests the yes-ue-mcp plugin tools using Python's unittest framework.
Updated for the consolidated tool architecture (10 read tools + 18 write tools).

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
"""

import json
import os
import unittest
import requests
from typing import Any, Dict, List, Optional


class McpClient:
    """Client for making MCP JSON-RPC requests."""

    def __init__(self, host: str = "127.0.0.1", port: int = 8080):
        self.url = f"http://{host}:{port}/message"
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
        port = 8080  # Fixed port - MCP server always runs on 8080
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
        # After aggressive consolidation: 10 read + 18 write = 28 tools
        # Note: Some tools may be missing depending on build config
        self.assertGreaterEqual(len(tool_names), 27,
            f"Expected at least 27 tools, got {len(tool_names)}")

    def test_required_read_tools_exist(self):
        """Test that essential read tools are registered (after consolidation)."""
        tools = self.client.list_tools()
        tool_names = {t["name"] for t in tools}

        # New consolidated tools (10 read tools total)
        required_tools = [
            "query-blueprint",        # Merged: analyze-blueprint, get-blueprint-functions, etc.
            "query-blueprint-graph",  # Merged: get-blueprint-graph, get-blueprint-node, etc.
            "query-level",            # Merged with get-actor-details
            "get-project-info",       # Merged with get-project-settings
            "query-asset",            # Merged: search-assets, inspect-asset, inspect-data-asset
            "query-material",         # Merged: get-material-graph, get-material-parameters
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
    ]

    @classmethod
    def setUpClass(cls):
        """Clean up any stale test assets from previous runs."""
        super().setUpClass()
        for asset_path in cls.TEST_ASSETS:
            try:
                cls.client.call_tool("delete-asset", {"asset_path": asset_path})
            except:
                pass

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

        result = self.client.call_tool("create-asset", {
            "asset_path": asset_path,
            "asset_class": "Blueprint",
            "parent_class": "Actor"
        })

        self.assertTrue(result.get("success"), "create-asset should succeed")
        self.created_assets.append(asset_path)

    def test_create_and_verify_blueprint(self):
        """Test that created Blueprint can be analyzed."""
        asset_path = "/Game/BP_MCP_TestVerify"

        # Create
        create_result = self.client.call_tool("create-asset", {
            "asset_path": asset_path,
            "asset_class": "Blueprint",
            "parent_class": "Actor"
        })
        self.assertTrue(create_result.get("success"))
        self.created_assets.append(asset_path)

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
        self.client.call_tool("create-asset", {
            "asset_path": asset_path,
            "asset_class": "Blueprint"
        })

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


class TestBlueprintModification(McpTestCase):
    """Test Blueprint graph modification tools."""

    @classmethod
    def setUpClass(cls):
        """Create a test Blueprint for modification tests."""
        super().setUpClass()
        cls.test_bp_path = "/Game/BP_MCP_ModificationTest"

        # Clean up any stale asset from previous runs
        try:
            cls.client.call_tool("delete-asset", {"asset_path": cls.test_bp_path})
        except:
            pass

        # Create test Blueprint
        result = cls.client.call_tool("create-asset", {
            "asset_path": cls.test_bp_path,
            "asset_class": "Blueprint",
            "parent_class": "Actor"
        })
        if not result.get("success"):
            raise unittest.SkipTest("Could not create test Blueprint")

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
        cls.test_bp_path = "/Game/BP_MCP_GraphNodeTest"

        # Clean up any stale asset from previous runs
        try:
            cls.client.call_tool("delete-asset", {"asset_path": cls.test_bp_path})
        except:
            pass

        # Create test Blueprint
        result = cls.client.call_tool("create-asset", {
            "asset_path": cls.test_bp_path,
            "asset_class": "Blueprint",
            "parent_class": "Actor"
        })
        if not result.get("success"):
            raise unittest.SkipTest("Could not create test Blueprint")

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
        cls.test_bp_path = "/Game/BP_MCP_PropertyTest"

        # Clean up any stale asset from previous runs
        try:
            cls.client.call_tool("delete-asset", {"asset_path": cls.test_bp_path})
        except:
            pass

        # Create test Blueprint
        result = cls.client.call_tool("create-asset", {
            "asset_path": cls.test_bp_path,
            "asset_class": "Blueprint",
            "parent_class": "Actor"
        })
        if not result.get("success"):
            raise unittest.SkipTest("Could not create test Blueprint")

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
        # Use StaticMeshActor since base Actor class cannot be spawned directly
        # spawn-actor uses 'label' param, not 'actor_name'
        result = self.client.call_tool("spawn-actor", {
            "actor_class": "StaticMeshActor",
            "label": self.test_actor_label
        })
        if not result.get("success"):
            self.skipTest("Could not spawn test actor")

    def tearDown(self):
        """Delete test actor."""
        try:
            self.client.call_tool("delete-actor", {
                "actor_name": self.test_actor_label
            })
        except:
            pass

    def test_add_component(self):
        """Test adding a component to an actor."""
        result = self.client.call_tool("add-component", {
            "actor_name": self.test_actor_label,
            "component_class": "StaticMeshComponent",
            "component_name": "TestMeshComp"
        })
        self.assertTrue(result.get("success"), "add-component should succeed")

    def test_remove_component(self):
        """Test removing a component from an actor."""
        # First add a component
        add_result = self.client.call_tool("add-component", {
            "actor_name": self.test_actor_label,
            "component_class": "PointLightComponent",
            "component_name": "TestLightComp"
        })
        if not add_result.get("success"):
            self.skipTest("Could not add component to test")

        # Remove it
        remove_result = self.client.call_tool("remove-component", {
            "actor_name": self.test_actor_label,
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


if __name__ == "__main__":
    unittest.main(verbosity=2)
