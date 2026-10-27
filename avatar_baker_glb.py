import bpy
import os
import glob

def convert_to_glb():
    tmp_dir = os.environ.get("AVATAR_TMP_DIR")
    out_path = os.environ.get("AVATAR_OUT_GLB")
    
    if not tmp_dir or not out_path:
        print("ERROR: Missing env vars")
        return
        
    # Clear existing objects
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.delete(use_global=False)
    
    # Find model files
    model_files = []
    for ext in ["*.fbx", "*.obj", "*.gltf", "*.glb"]:
        model_files.extend(glob.glob(os.path.join(tmp_dir, "**", ext), recursive=True))
        
    if not model_files:
        print("ERROR: No 3D model found in", tmp_dir)
        return
        
    in_path = model_files[0]
    print(f"Importing {in_path}...")
    
    ext = os.path.splitext(in_path)[1].lower()
    if ext == ".fbx":
        bpy.ops.import_scene.fbx(filepath=in_path)
    elif ext == ".obj":
        bpy.ops.import_scene.obj(filepath=in_path)
    elif ext in [".gltf", ".glb"]:
        bpy.ops.import_scene.gltf(filepath=in_path)
        
    # Export to GLB
    print(f"Exporting to {out_path}...")
    bpy.ops.export_scene.gltf(
        filepath=out_path,
        export_format='GLB',
        export_animations=True,
        export_apply=True
    )
    print("Done!")

if __name__ == "__main__":
    convert_to_glb()
