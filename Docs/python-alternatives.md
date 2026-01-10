# Python Script Alternatives

These operations were previously dedicated MCP tools but are now done via the `run-python-script` tool.

## Save Asset

Previously: `save-asset` tool

```python
import unreal

# Save a single asset
unreal.EditorAssetLibrary.save_asset('/Game/Blueprints/BP_MyActor')

# Save with optional parameters
unreal.EditorAssetLibrary.save_asset('/Game/Blueprints/BP_MyActor', only_if_is_dirty=True)
```

## Delete Asset

Previously: `delete-asset` tool

```python
import unreal

# Delete a single asset
unreal.EditorAssetLibrary.delete_asset('/Game/Blueprints/BP_OldActor')

# Check if asset exists first
if unreal.EditorAssetLibrary.does_asset_exist('/Game/Blueprints/BP_OldActor'):
    unreal.EditorAssetLibrary.delete_asset('/Game/Blueprints/BP_OldActor')
```

## Compile Blueprint

Previously: `compile-blueprint` tool

```python
import unreal

# Load and compile a Blueprint
bp = unreal.load_asset('/Game/Blueprints/BP_MyActor')
unreal.KismetEditorUtilities.compile_blueprint(bp)

# Check compilation status
if bp.get_editor_property('status') == unreal.BlueprintStatus.BS_UP_TO_DATE:
    print('Blueprint compiled successfully')
```

## Delete Actor

Previously: `delete-actor` tool

```python
import unreal

# Delete actor by label
actors = unreal.EditorLevelLibrary.get_all_level_actors()
for actor in actors:
    if actor.get_actor_label() == 'MyActorLabel':
        actor.destroy_actor()
        break

# Delete actor by class
for actor in actors:
    if actor.get_class().get_name() == 'BP_Enemy_C':
        actor.destroy_actor()
        break

# Delete all actors of a type
for actor in actors:
    if isinstance(actor, unreal.PointLight):
        actor.destroy_actor()
```

## Remove Component

Previously: `remove-component` tool

```python
import unreal

# Find actor and remove component by class
actors = unreal.EditorLevelLibrary.get_all_level_actors()
for actor in actors:
    if actor.get_actor_label() == 'MyActor':
        # Remove by component class
        component = actor.get_component_by_class(unreal.StaticMeshComponent)
        if component:
            component.destroy_component()
        break

# Remove component by name
for actor in actors:
    if actor.get_actor_label() == 'MyActor':
        components = actor.get_components_by_class(unreal.SceneComponent)
        for comp in components:
            if comp.get_name() == 'MyComponentName':
                comp.destroy_component()
                break
        break
```

## Remove Widget

Previously: `remove-widget` tool

```python
import unreal

# Load WidgetBlueprint and modify widget tree
widget_bp = unreal.load_asset('/Game/UI/WBP_MainMenu')
widget_tree = widget_bp.get_editor_property('widget_tree')

# Widget tree manipulation requires accessing the widget tree's root
# and traversing to find and remove specific widgets
root_widget = widget_tree.get_editor_property('root_widget')
# Note: Widget removal through Python requires careful tree manipulation
```

## Remove DataTable Row

Previously: `remove-datatable-row` tool

```python
import unreal

# Load DataTable and remove a row
dt = unreal.load_asset('/Game/Data/DT_Items')
dt.remove_row('RowName')

# Verify row was removed
row_names = dt.get_row_names()
if 'RowName' not in row_names:
    print('Row removed successfully')
```

## Usage via run-python-script

To execute any of these scripts via MCP:

```json
{
  "tool": "run-python-script",
  "script": "import unreal\nunreal.EditorAssetLibrary.save_asset('/Game/MyBlueprint')"
}
```

Or with arguments:

```json
{
  "tool": "run-python-script",
  "script": "import unreal\nargs = unreal.get_mcp_args()\nunreal.EditorAssetLibrary.save_asset(args['asset_path'])",
  "arguments": {
    "asset_path": "/Game/Blueprints/BP_Player"
  }
}
```
