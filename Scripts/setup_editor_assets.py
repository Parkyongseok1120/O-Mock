"""Create the editor-facing prototype assets for O-Mock.

Run with:
    UnrealEditor-Cmd.exe O_Mock.uproject -ExecutePythonScript=Scripts/setup_editor_assets.py

The script is intentionally idempotent. Existing assets are reused and their
project-specific defaults are refreshed instead of being duplicated.
"""

import unreal


ROOT = "/Game/O_Mock"
MAP_PATH = "/Game/NewWorld"

FOLDERS = (
    f"{ROOT}/Blueprints/Game",
    f"{ROOT}/Blueprints/Board",
    f"{ROOT}/Blueprints/UI",
    f"{ROOT}/Data/BoardTemplates",
    f"{ROOT}/Materials/Board",
    f"{ROOT}/Materials/Stones",
    f"{ROOT}/Meshes/Board",
    f"{ROOT}/Meshes/Stones",
    f"{ROOT}/UI/Widgets",
    f"{ROOT}/UI/Textures",
    f"{ROOT}/UI/Icons",
    f"{ROOT}/UI/Fonts",
    f"{ROOT}/Audio",
    f"{ROOT}/FX",
    f"{ROOT}/Maps",
)

BOARD_TEMPLATE_PATH = f"{ROOT}/Data/BoardTemplates/DA_Board_Default15"
TEST_TEMPLATE_PATH = f"{ROOT}/Data/BoardTemplates/DA_Board_TestBlocked15"
LARGE_TEMPLATE_PATH = f"{ROOT}/Data/BoardTemplates/DA_Board_Default21"
GAME_MODE_PATH = f"{ROOT}/Blueprints/Game/BP_GM_Prototype"
BOARD_BLUEPRINT_PATH = f"{ROOT}/Blueprints/Board/BP_GomokuBoard_Prototype"
HUD_BLUEPRINT_PATH = f"{ROOT}/Blueprints/UI/BP_HUD_Prototype"
HUD_WIDGET_PATH = f"{ROOT}/UI/Widgets/WBP_GameHUD"
HUD_TEXTURE_PATH = f"{ROOT}/UI/Textures/T_HUD_Panel"


def log(message):
    unreal.log(f"[O-Mock Setup] {message}")


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def ensure_folders():
    for folder in FOLDERS:
        if not unreal.EditorAssetLibrary.does_directory_exist(folder):
            require(
                unreal.EditorAssetLibrary.make_directory(folder),
                f"Could not create content folder: {folder}",
            )


def load_asset(asset_path):
    return unreal.EditorAssetLibrary.load_asset(asset_path)


def save_asset(asset):
    require(asset is not None, "Cannot save an unloaded asset")
    require(
        unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False),
        f"Could not save asset: {asset.get_path_name()}",
    )


def create_or_load_data_asset(asset_path, template_id, width, height, blocked_cells):
    asset = load_asset(asset_path)
    if asset is None:
        package_path, asset_name = asset_path.rsplit("/", 1)
        factory = unreal.DataAssetFactory()
        asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            asset_name,
            package_path,
            unreal.GomokuBoardTemplateDataAsset,
            factory,
        )
        require(asset is not None, f"Could not create board template: {asset_path}")
        log(f"Created {asset_path}")
    else:
        require(
            isinstance(asset, unreal.GomokuBoardTemplateDataAsset),
            f"Unexpected asset type at {asset_path}",
        )

    asset.set_editor_property("template_id", template_id)
    asset.set_editor_property("width", width)
    asset.set_editor_property("height", height)
    asset.set_editor_property("blocked_cells", blocked_cells)
    save_asset(asset)
    return asset


def create_material(asset_path, parameter_name, color, roughness):
    material = load_asset(asset_path)
    if material is None:
        package_path, asset_name = asset_path.rsplit("/", 1)
        material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            asset_name,
            package_path,
            unreal.Material,
            unreal.MaterialFactoryNew(),
        )
        require(material is not None, f"Could not create material: {asset_path}")
        log(f"Created {asset_path}")
    else:
        require(isinstance(material, unreal.Material), f"Unexpected asset type at {asset_path}")

    expressions = unreal.MaterialEditingLibrary.get_material_expressions(material)
    color_parameter = next(
        (
            expression
            for expression in expressions
            if isinstance(expression, unreal.MaterialExpressionVectorParameter)
            and str(expression.get_editor_property("parameter_name")) == parameter_name
        ),
        None,
    )
    if color_parameter is None:
        color_parameter = unreal.MaterialEditingLibrary.create_material_expression(
            material,
            unreal.MaterialExpressionVectorParameter,
            -300,
            -80,
        )
        require(color_parameter is not None, f"Could not create Color parameter for {asset_path}")
    color_parameter.set_editor_property("parameter_name", parameter_name)
    color_parameter.set_editor_property("default_value", color)
    require(
        unreal.MaterialEditingLibrary.connect_material_property(
            color_parameter,
            "",
            unreal.MaterialProperty.MP_BASE_COLOR,
        ),
        f"Could not connect Base Color for {asset_path}",
    )
    # The first prototype used emissive output because the map had no lights.
    # The 3D presentation supplies a sun/sky rig, so emissive must be disconnected
    # for highlights, shadows and depth cues to render correctly.
    unreal.MaterialEditingLibrary.disconnect_material_property(
        material,
        unreal.MaterialProperty.MP_EMISSIVE_COLOR,
    )

    roughness_parameter = next(
        (
            expression
            for expression in expressions
            if isinstance(expression, unreal.MaterialExpressionScalarParameter)
            and str(expression.get_editor_property("parameter_name")) == "Roughness"
        ),
        None,
    )
    if roughness_parameter is None:
        roughness_parameter = unreal.MaterialEditingLibrary.create_material_expression(
            material,
            unreal.MaterialExpressionScalarParameter,
            -300,
            80,
        )
        require(roughness_parameter is not None, f"Could not create Roughness parameter for {asset_path}")
    roughness_parameter.set_editor_property("parameter_name", "Roughness")
    roughness_parameter.set_editor_property("default_value", roughness)
    require(
        unreal.MaterialEditingLibrary.connect_material_property(
            roughness_parameter,
            "",
            unreal.MaterialProperty.MP_ROUGHNESS,
        ),
        f"Could not connect Roughness for {asset_path}",
    )

    unreal.MaterialEditingLibrary.layout_material_expressions(material)
    unreal.MaterialEditingLibrary.recompile_material(material)
    save_asset(material)
    return material


def enable_instanced_static_mesh_usage(material):
    material.set_editor_property("used_with_instanced_static_meshes", True)
    unreal.MaterialEditingLibrary.recompile_material(material)
    save_asset(material)


def import_hud_texture():
    texture = load_asset(HUD_TEXTURE_PATH)
    if texture is None:
        source_filename = unreal.Paths.convert_relative_path_to_full(
            unreal.Paths.project_dir() + "SourceArt/UI/T_HUD_Panel.png"
        )
        require(unreal.Paths.file_exists(source_filename), f"Missing HUD source art: {source_filename}")
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", source_filename)
        task.set_editor_property("destination_path", f"{ROOT}/UI/Textures")
        task.set_editor_property("destination_name", "T_HUD_Panel")
        task.set_editor_property("automated", True)
        task.set_editor_property("replace_existing", False)
        task.set_editor_property("save", True)
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        texture = load_asset(HUD_TEXTURE_PATH)
        require(texture is not None, f"Could not import HUD texture: {HUD_TEXTURE_PATH}")
        log(f"Imported {HUD_TEXTURE_PATH}")

    require(isinstance(texture, unreal.Texture2D), f"Unexpected asset type at {HUD_TEXTURE_PATH}")
    texture.set_editor_property("never_stream", True)
    save_asset(texture)
    return texture


def create_or_load_blueprint(asset_path, parent_class):
    blueprint = load_asset(asset_path)
    if blueprint is None:
        package_path, asset_name = asset_path.rsplit("/", 1)
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", parent_class)
        blueprint = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            asset_name,
            package_path,
            unreal.Blueprint,
            factory,
        )
        require(blueprint is not None, f"Could not create Blueprint: {asset_path}")
        log(f"Created {asset_path}")
    else:
        require(isinstance(blueprint, unreal.Blueprint), f"Unexpected asset type at {asset_path}")

    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    return blueprint


def create_or_load_widget_blueprint(asset_path, parent_class):
    blueprint = load_asset(asset_path)
    if blueprint is None:
        package_path, asset_name = asset_path.rsplit("/", 1)
        factory = unreal.WidgetBlueprintFactory()
        factory.set_editor_property("parent_class", parent_class)
        blueprint = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            asset_name,
            package_path,
            unreal.WidgetBlueprint,
            factory,
        )
        require(blueprint is not None, f"Could not create Widget Blueprint: {asset_path}")
        log(f"Created {asset_path}")
    else:
        require(isinstance(blueprint, unreal.WidgetBlueprint), f"Unexpected asset type at {asset_path}")

    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    return blueprint


def find_source_widget(widget_blueprint, widget_name):
    return unreal.EditorUtilityLibrary.find_source_widget_by_name(
        widget_blueprint,
        unreal.Name(widget_name),
    )


def ensure_source_widget(widget_blueprint, widget_class, widget_name, parent_name=""):
    widget = find_source_widget(widget_blueprint, widget_name)
    if widget is not None:
        return widget

    widget = unreal.EditorUtilityLibrary.add_source_widget(
        widget_blueprint,
        widget_class,
        unreal.Name(widget_name),
        unreal.Name(parent_name),
    )
    require(widget is not None, f"Could not create source widget: {widget_name}")
    return widget


def configure_canvas_widget(widget, anchor_x, anchor_y, position_x, position_y, width, height,
                            alignment_x=0.0, alignment_y=0.0):
    slot = widget.get_editor_property("slot")
    require(isinstance(slot, unreal.CanvasPanelSlot), f"{widget.get_name()} is not in RootCanvas")
    anchor = unreal.Vector2D(anchor_x, anchor_y)
    slot.set_anchors(unreal.Anchors(anchor, anchor))
    slot.set_alignment(unreal.Vector2D(alignment_x, alignment_y))
    slot.set_position(unreal.Vector2D(position_x, position_y))
    slot.set_size(unreal.Vector2D(width, height))
    return slot


def configure_hud_widget_tree(widget_blueprint, hud_texture):
    root = ensure_source_widget(widget_blueprint, unreal.CanvasPanel, "RootCanvas")
    require(isinstance(root, unreal.CanvasPanel), "WBP_GameHUD root is not a CanvasPanel")

    top_background = ensure_source_widget(
        widget_blueprint, unreal.Border, "TopBackground", "RootCanvas"
    )
    top_background.set_brush_color(unreal.LinearColor(0.018, 0.035, 0.03, 0.96))
    configure_canvas_widget(top_background, 0.5, 0.0, 0.0, 18.0, 720.0, 118.0, 0.5, 0.0)

    top_image = ensure_source_widget(widget_blueprint, unreal.Image, "TopPanelImage", "RootCanvas")
    top_image.set_brush_from_texture(hud_texture, True)
    top_image.set_color_and_opacity(unreal.LinearColor(1.0, 1.0, 1.0, 0.94))
    configure_canvas_widget(top_image, 0.5, 0.0, 0.0, 18.0, 720.0, 118.0, 0.5, 0.0)

    headline = ensure_source_widget(widget_blueprint, unreal.TextBlock, "HeadlineText", "RootCanvas")
    headline.set_text("LOADING MATCH")
    headline.set_editor_property("justification", unreal.TextJustify.CENTER)
    configure_canvas_widget(headline, 0.5, 0.0, 0.0, 42.0, 650.0, 34.0, 0.5, 0.0)

    phase = ensure_source_widget(widget_blueprint, unreal.TextBlock, "PhaseText", "RootCanvas")
    phase.set_text("CONNECTING TO MATCH STATE")
    phase.set_editor_property("justification", unreal.TextJustify.CENTER)
    configure_canvas_widget(phase, 0.5, 0.0, 0.0, 88.0, 650.0, 24.0, 0.5, 0.0)

    for player_index in range(4):
        card_name = f"PlayerCard{player_index + 1}"
        card = ensure_source_widget(widget_blueprint, unreal.Border, card_name, "RootCanvas")
        card.set_padding(unreal.Margin(14.0, 9.0, 14.0, 9.0))
        card.set_brush_color(unreal.LinearColor(0.025, 0.04, 0.035, 0.92))
        card_text = ensure_source_widget(
            widget_blueprint,
            unreal.TextBlock,
            f"PlayerCardText{player_index + 1}",
            card_name,
        )
        card_text.set_text(f"PLAYER {player_index + 1}\\nWAITING")
        configure_canvas_widget(
            card,
            0.0,
            0.5,
            22.0,
            -145.0 + player_index * 73.0,
            286.0,
            64.0,
            0.0,
            0.0,
        )

    inventory_background = ensure_source_widget(
        widget_blueprint, unreal.Border, "InventoryBackground", "RootCanvas"
    )
    inventory_background.set_brush_color(unreal.LinearColor(0.015, 0.028, 0.024, 0.96))
    configure_canvas_widget(inventory_background, 0.5, 1.0, 0.0, -22.0, 760.0, 116.0, 0.5, 1.0)

    inventory = ensure_source_widget(widget_blueprint, unreal.TextBlock, "InventoryText", "RootCanvas")
    inventory.set_text("ENERGY 0/10")
    inventory.set_editor_property("justification", unreal.TextJustify.CENTER)
    configure_canvas_widget(inventory, 0.5, 1.0, 0.0, -112.0, 720.0, 34.0, 0.5, 1.0)

    help_text = ensure_source_widget(widget_blueprint, unreal.TextBlock, "HelpText", "RootCanvas")
    help_text.set_text("LMB PLACE  ·  RMB DRAG ORBIT  ·  WHEEL ZOOM  ·  F RESET VIEW")
    help_text.set_editor_property("justification", unreal.TextJustify.CENTER)
    configure_canvas_widget(help_text, 0.5, 1.0, 0.0, -67.0, 720.0, 28.0, 0.5, 1.0)

    game_over_panel = ensure_source_widget(
        widget_blueprint, unreal.Border, "GameOverPanel", "RootCanvas"
    )
    game_over_panel.set_brush_color(unreal.LinearColor(0.02, 0.055, 0.045, 0.99))
    game_over_panel.set_padding(unreal.Margin(30.0, 30.0, 30.0, 30.0))
    game_over_panel.set_visibility(unreal.SlateVisibility.COLLAPSED)
    game_over_text = ensure_source_widget(
        widget_blueprint, unreal.TextBlock, "GameOverText", "GameOverPanel"
    )
    game_over_text.set_text("GAME OVER")
    game_over_text.set_editor_property("justification", unreal.TextJustify.CENTER)
    game_over_text.set_editor_property("auto_wrap_text", True)
    configure_canvas_widget(game_over_panel, 0.5, 0.5, 0.0, 0.0, 560.0, 158.0, 0.5, 0.5)

    log("Configured saved Designer WidgetTree for WBP_GameHUD")


def set_component_material(component, material, component_name):
    require(component is not None, f"Missing inherited component: {component_name}")
    component.set_material(0, material)


def configure_blueprints(
    default_template,
    board_material,
    stone_material,
    grid_material,
    blocked_material,
    frame_material,
    hover_material,
    ground_material,
    pedestal_material,
    hill_material,
    trunk_material,
    canopy_material,
    hud_texture,
):
    hud_widget_blueprint = create_or_load_widget_blueprint(HUD_WIDGET_PATH, unreal.GomokuHUDWidget)
    configure_hud_widget_tree(hud_widget_blueprint, hud_texture)
    unreal.BlueprintEditorLibrary.compile_blueprint(hud_widget_blueprint)
    hud_widget_cdo = unreal.get_default_object(hud_widget_blueprint.generated_class())
    hud_widget_cdo.set_editor_property("panel_texture", hud_texture)
    save_asset(hud_widget_blueprint)

    hud_blueprint = create_or_load_blueprint(HUD_BLUEPRINT_PATH, unreal.GomokuHUD)
    hud_cdo = unreal.get_default_object(hud_blueprint.generated_class())
    hud_cdo.set_editor_property("hud_widget_class", hud_widget_blueprint.generated_class())
    save_asset(hud_blueprint)

    game_mode_blueprint = create_or_load_blueprint(GAME_MODE_PATH, unreal.GomokuGameMode)
    game_mode_cdo = unreal.get_default_object(game_mode_blueprint.generated_class())
    game_mode_cdo.set_editor_property("default_hotseat_players", 2)
    game_mode_cdo.set_editor_property("max_lobby_players", 4)
    game_mode_cdo.set_editor_property("board_template", default_template)
    game_mode_cdo.set_editor_property("hud_class", hud_blueprint.generated_class())
    save_asset(game_mode_blueprint)

    board_blueprint = create_or_load_blueprint(BOARD_BLUEPRINT_PATH, unreal.GomokuBoardActor)
    board_cdo = unreal.get_default_object(board_blueprint.generated_class())
    board_cdo.set_editor_property("cell_size", 60.0)

    # Clear serialized orthographic-camera overrides left by the earlier flat prototype.
    board_camera = board_cdo.get_editor_property("board_camera")
    board_camera.set_editor_property("relative_location", unreal.Vector(0.0, 0.0, 0.0))
    board_camera.set_editor_property("relative_rotation", unreal.Rotator(0.0, 0.0, 0.0))
    board_camera.set_editor_property("projection_mode", unreal.CameraProjectionMode.PERSPECTIVE)
    board_camera.set_editor_property("field_of_view", 55.0)
    camera_boom = board_cdo.get_editor_property("camera_boom")
    camera_boom.set_editor_property("relative_location", unreal.Vector(0.0, 0.0, 70.0))
    boom_rotation = unreal.Rotator()
    boom_rotation.set_editor_property("pitch", -24.0)
    boom_rotation.set_editor_property("yaw", -35.0)
    boom_rotation.set_editor_property("roll", 0.0)
    camera_boom.set_editor_property("relative_rotation", boom_rotation)
    camera_boom.set_editor_property("target_arm_length", 2400.0)

    set_component_material(board_cdo.get_editor_property("board_plane"), board_material, "BoardPlane")
    for property_name in (
        "stone_instances",
        "stone_instances_player2",
        "stone_instances_player3",
        "stone_instances_player4",
    ):
        set_component_material(board_cdo.get_editor_property(property_name), stone_material, property_name)
    set_component_material(
        board_cdo.get_editor_property("board_grid_instances"),
        grid_material,
        "BoardGridInstances",
    )
    set_component_material(
        board_cdo.get_editor_property("blocked_cell_instances"),
        blocked_material,
        "BlockedCellInstances",
    )
    set_component_material(
        board_cdo.get_editor_property("board_frame_instances"),
        frame_material,
        "BoardFrameInstances",
    )
    set_component_material(
        board_cdo.get_editor_property("hover_cell_instances"),
        hover_material,
        "HoverCellInstances",
    )
    set_component_material(board_cdo.get_editor_property("ground_plane"), ground_material, "GroundPlane")
    set_component_material(
        board_cdo.get_editor_property("board_pedestal"), pedestal_material, "BoardPedestal"
    )
    set_component_material(board_cdo.get_editor_property("hill_instances"), hill_material, "HillInstances")
    set_component_material(
        board_cdo.get_editor_property("tree_trunk_instances"), trunk_material, "TreeTrunkInstances"
    )
    set_component_material(
        board_cdo.get_editor_property("tree_canopy_instances"), canopy_material, "TreeCanopyInstances"
    )
    save_asset(board_blueprint)

    unreal.BlueprintEditorLibrary.compile_blueprint(game_mode_blueprint)
    unreal.BlueprintEditorLibrary.compile_blueprint(board_blueprint)
    unreal.BlueprintEditorLibrary.compile_blueprint(hud_blueprint)
    unreal.BlueprintEditorLibrary.compile_blueprint(hud_widget_blueprint)
    save_asset(game_mode_blueprint)
    save_asset(board_blueprint)
    save_asset(hud_blueprint)
    save_asset(hud_widget_blueprint)
    return game_mode_blueprint, board_blueprint, hud_blueprint, hud_widget_blueprint


def place_board_in_map(board_blueprint):
    world = unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH)
    require(world is not None, f"Could not load map: {MAP_PATH}")

    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    board_class = board_blueprint.generated_class()
    board_actors = [
        actor
        for actor in actor_subsystem.get_all_level_actors()
        if isinstance(actor, unreal.GomokuBoardActor)
    ]

    board_actor = None
    for actor in board_actors:
        if actor.get_class() == board_class and board_actor is None:
            board_actor = actor
        else:
            actor_subsystem.destroy_actor(actor)

    if board_actor is None:
        board_actor = actor_subsystem.spawn_actor_from_class(
            board_class,
            unreal.Vector(0.0, 0.0, 0.0),
            unreal.Rotator(0.0, 0.0, 0.0),
        )
        require(board_actor is not None, "Could not place BP_GomokuBoard_Prototype in NewWorld")
        log("Placed BP_GomokuBoard_Prototype in NewWorld")

    board_actor.set_actor_label("GomokuBoard_Prototype")
    board_actor.set_actor_location(unreal.Vector(0.0, 0.0, 0.0), False, False)
    board_actor.set_actor_rotation(unreal.Rotator(0.0, 0.0, 0.0), False)
    board_actor.set_actor_scale3d(unreal.Vector(1.0, 1.0, 1.0))
    board_actor.set_editor_property("cell_size", 60.0)

    world_settings = world.get_world_settings()
    world_settings.set_editor_property("default_game_mode", None)
    require(
        unreal.EditorLoadingAndSavingUtils.save_map(world, MAP_PATH),
        f"Could not save map: {MAP_PATH}",
    )
    return world


def validate_assets(
    game_mode_blueprint,
    board_blueprint,
    hud_blueprint,
    hud_widget_blueprint,
    default_template,
    materials,
    hud_texture,
    world,
):
    require(default_template.get_editor_property("width") == 15, "Default template width is not 15")
    require(default_template.get_editor_property("height") == 15, "Default template height is not 15")

    game_mode_cdo = unreal.get_default_object(game_mode_blueprint.generated_class())
    require(
        game_mode_cdo.get_editor_property("board_template") == default_template,
        "GameMode Blueprint is not linked to DA_Board_Default15",
    )
    require(game_mode_cdo.get_editor_property("default_hotseat_players") == 2, "Hotseat default is not 2")
    require(game_mode_cdo.get_editor_property("max_lobby_players") == 4, "Lobby maximum is not 4")
    require(
        game_mode_cdo.get_editor_property("hud_class") == hud_blueprint.generated_class(),
        "GameMode Blueprint is not linked to BP_HUD_Prototype",
    )

    hud_cdo = unreal.get_default_object(hud_blueprint.generated_class())
    require(
        hud_cdo.get_editor_property("hud_widget_class") == hud_widget_blueprint.generated_class(),
        "HUD Blueprint is not linked to WBP_GameHUD",
    )
    hud_widget_cdo = unreal.get_default_object(hud_widget_blueprint.generated_class())
    require(
        hud_widget_cdo.get_editor_property("panel_texture") == hud_texture,
        "WBP_GameHUD is not linked to T_HUD_Panel",
    )

    board_cdo = unreal.get_default_object(board_blueprint.generated_class())
    require(abs(board_cdo.get_editor_property("cell_size") - 60.0) < 0.01, "Board CellSize is not 60")
    component_materials = (
        (board_cdo.get_editor_property("board_plane"), materials[0], "BoardPlane"),
        (board_cdo.get_editor_property("stone_instances"), materials[1], "StoneInstances"),
        (board_cdo.get_editor_property("board_grid_instances"), materials[2], "BoardGridInstances"),
        (board_cdo.get_editor_property("blocked_cell_instances"), materials[3], "BlockedCellInstances"),
        (board_cdo.get_editor_property("board_frame_instances"), materials[4], "BoardFrameInstances"),
        (board_cdo.get_editor_property("hover_cell_instances"), materials[5], "HoverCellInstances"),
        (board_cdo.get_editor_property("ground_plane"), materials[6], "GroundPlane"),
        (board_cdo.get_editor_property("board_pedestal"), materials[7], "BoardPedestal"),
        (board_cdo.get_editor_property("hill_instances"), materials[8], "HillInstances"),
        (board_cdo.get_editor_property("tree_trunk_instances"), materials[9], "TreeTrunkInstances"),
        (board_cdo.get_editor_property("tree_canopy_instances"), materials[10], "TreeCanopyInstances"),
    )
    for component, expected_material, component_name in component_materials:
        require(component.get_material(0) == expected_material, f"{component_name} material was not saved")

    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    placed_boards = [
        actor
        for actor in actor_subsystem.get_all_level_actors()
        if actor.get_class() == board_blueprint.generated_class()
    ]
    require(len(placed_boards) == 1, "NewWorld must contain exactly one prototype board")
    placed_board = placed_boards[0]
    grid_component = placed_board.get_editor_property("board_grid_instances")
    frame_component = placed_board.get_editor_property("board_frame_instances")
    board_plane = placed_board.get_editor_property("board_plane")
    board_scale = board_plane.get_editor_property("relative_scale3d")
    require(grid_component.get_instance_count() == 32, "Editor preview grid must contain 32 lines for 15x15")
    require(frame_component.get_instance_count() == 4, "Editor preview board frame must contain four rails")
    require(
        placed_board.get_editor_property("hill_instances").get_instance_count() == 18,
        "Editor preview environment must contain 18 distant hills",
    )
    require(
        placed_board.get_editor_property("tree_canopy_instances").get_instance_count() == 36,
        "Editor preview environment must contain 36 trees",
    )
    require(
        abs(board_scale.x - 9.0) < 0.01 and abs(board_scale.y - 9.0) < 0.01,
        "Editor preview board must be scaled to 900x900 Unreal units",
    )
    require(world.get_world_settings().get_editor_property("default_game_mode") is None, "Map GameMode override is not clear")

    for asset_path in (
        BOARD_TEMPLATE_PATH,
        TEST_TEMPLATE_PATH,
        LARGE_TEMPLATE_PATH,
        GAME_MODE_PATH,
        BOARD_BLUEPRINT_PATH,
        HUD_BLUEPRINT_PATH,
        HUD_WIDGET_PATH,
        HUD_TEXTURE_PATH,
    ):
        require(unreal.EditorAssetLibrary.does_asset_exist(asset_path), f"Missing generated asset: {asset_path}")


def main():
    log("Starting editor asset setup")
    ensure_folders()

    default_template = create_or_load_data_asset(
        BOARD_TEMPLATE_PATH,
        "Default15",
        15,
        15,
        [],
    )
    create_or_load_data_asset(
        TEST_TEMPLATE_PATH,
        "TestBlocked15",
        15,
        15,
        [unreal.IntPoint(3, 3), unreal.IntPoint(7, 7), unreal.IntPoint(11, 11)],
    )
    create_or_load_data_asset(
        LARGE_TEMPLATE_PATH,
        "Default21",
        21,
        21,
        [],
    )

    board_material = create_material(
        f"{ROOT}/Materials/Board/M_Board_Master",
        "BaseColor",
        unreal.LinearColor(0.76, 0.48, 0.20, 1.0),
        0.65,
    )
    stone_material = create_material(
        f"{ROOT}/Materials/Stones/M_Stone_Master",
        "Color",
        unreal.LinearColor(0.92, 0.92, 0.92, 1.0),
        0.28,
    )
    grid_material = create_material(
        f"{ROOT}/Materials/Board/M_Grid_Master",
        "Color",
        unreal.LinearColor(0.12, 0.08, 0.035, 1.0),
        0.75,
    )
    blocked_material = create_material(
        f"{ROOT}/Materials/Board/M_BlockedCell_Master",
        "Color",
        unreal.LinearColor(0.45, 0.08, 0.06, 1.0),
        0.60,
    )
    frame_material = create_material(
        f"{ROOT}/Materials/Board/M_BoardFrame_Master",
        "Color",
        unreal.LinearColor(0.18, 0.07, 0.025, 1.0),
        0.35,
    )
    hover_material = create_material(
        f"{ROOT}/Materials/Board/M_Hover_Master",
        "Color",
        unreal.LinearColor(1.0, 0.68, 0.12, 1.0),
        0.24,
    )
    ground_material = create_material(
        f"{ROOT}/Materials/Environment/M_Ground_Master",
        "Color",
        unreal.LinearColor(0.08, 0.26, 0.105, 1.0),
        0.92,
    )
    pedestal_material = create_material(
        f"{ROOT}/Materials/Environment/M_Pedestal_Master",
        "Color",
        unreal.LinearColor(0.16, 0.075, 0.032, 1.0),
        0.58,
    )
    hill_material = create_material(
        f"{ROOT}/Materials/Environment/M_Hills_Master",
        "Color",
        unreal.LinearColor(0.12, 0.24, 0.16, 1.0),
        0.96,
    )
    trunk_material = create_material(
        f"{ROOT}/Materials/Environment/M_TreeTrunk_Master",
        "Color",
        unreal.LinearColor(0.20, 0.085, 0.035, 1.0),
        0.88,
    )
    canopy_material = create_material(
        f"{ROOT}/Materials/Environment/M_TreeCanopy_Master",
        "Color",
        unreal.LinearColor(0.055, 0.30, 0.12, 1.0),
        0.84,
    )
    for instanced_material in (
        stone_material,
        grid_material,
        blocked_material,
        frame_material,
        hover_material,
        hill_material,
        trunk_material,
        canopy_material,
    ):
        enable_instanced_static_mesh_usage(instanced_material)
    hud_texture = import_hud_texture()

    game_mode_blueprint, board_blueprint, hud_blueprint, hud_widget_blueprint = configure_blueprints(
        default_template,
        board_material,
        stone_material,
        grid_material,
        blocked_material,
        frame_material,
        hover_material,
        ground_material,
        pedestal_material,
        hill_material,
        trunk_material,
        canopy_material,
        hud_texture,
    )
    world = place_board_in_map(board_blueprint)
    validate_assets(
        game_mode_blueprint,
        board_blueprint,
        hud_blueprint,
        hud_widget_blueprint,
        default_template,
        (
            board_material,
            stone_material,
            grid_material,
            blocked_material,
            frame_material,
            hover_material,
            ground_material,
            pedestal_material,
            hill_material,
            trunk_material,
            canopy_material,
        ),
        hud_texture,
        world,
    )

    unreal.EditorAssetLibrary.save_directory(ROOT, only_if_is_dirty=False, recursive=True)
    log("SETUP_OK")


if __name__ == "__main__":
    main()
