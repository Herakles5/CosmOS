import bpy
import struct
import os
import glob
import mathutils

def bake_to_a3d():
    # Find the glTF file recursively
    avatar_dir = "/root/coding/MeinOS/avatar/"
    gltf_files = glob.glob(os.path.join(avatar_dir, "**/*.gltf"), recursive=True) + glob.glob(os.path.join(avatar_dir, "**/*.glb"), recursive=True)
    if not gltf_files:
        print("ERROR: No .gltf or .glb file found in", avatar_dir)
        return
    
    gltf_path = gltf_files[0]
    out_path = os.path.join(avatar_dir, "avatar.a3d")
    
    # Clear existing objects
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.delete(use_global=False)
    
    # Import glTF
    print(f"Importing {gltf_path}...")
    bpy.ops.import_scene.gltf(filepath=gltf_path)
    
    # Setup animation context
    scene = bpy.context.scene
    start_frame = scene.frame_start
    end_frame = scene.frame_end
    
    print(f"Animation from frame {start_frame} to {end_frame}")
    
    # Prepare output file
    with open(out_path, "wb") as f:
        # Header: "A3D!" (4 bytes), num_frames (4 bytes), res (8 bytes)
        num_frames = end_frame - start_frame + 1
        f.write(struct.pack("<4sIII", b"A3D!", num_frames, 0, 0))
        
        depsgraph = bpy.context.evaluated_depsgraph_get()
        
        for frame in range(start_frame, end_frame + 1):
            scene.frame_set(frame)
            depsgraph.update()
            
            # Gather all triangles for this frame
            triangles = []
            
            for obj in scene.objects:
                if obj.type == 'MESH':
                    eval_obj = obj.evaluated_get(depsgraph)
                    mesh = eval_obj.to_mesh()
                    
                    # Ensure triangles
                    import bmesh
                    bm = bmesh.new()
                    bm.from_mesh(mesh)
                    bmesh.ops.triangulate(bm, faces=bm.faces)
                    bm.to_mesh(mesh)
                    bm.free()
                    
                    mesh.calc_loop_triangles()
                    
                    # Get UV layer
                    uv_layer = mesh.uv_layers.active.data if mesh.uv_layers.active else None
                    
                    transform = obj.matrix_world
                    
                    for tri in mesh.loop_triangles:
                        tri_data = []
                        for i in range(3):
                            loop_idx = tri.loops[i]
                            vert_idx = tri.vertices[i]
                            
                            # Position
                            co = transform @ mesh.vertices[vert_idx].co
                            
                            # UV
                            u, v = 0.0, 0.0
                            if uv_layer:
                                uv = uv_layer[loop_idx].uv
                                u, v = uv[0], uv[1]
                                
                            tri_data.extend([co.x, co.y, co.z, u, v])
                        triangles.append(tri_data)
                    
                    eval_obj.to_mesh_clear()
            
            # Write frame data
            # num_triangles (4 bytes)
            f.write(struct.pack("<I", len(triangles)))
            # tri data (15 floats per tri)
            for tri_data in triangles:
                f.write(struct.pack("<15f", *tri_data))
                
            if frame % 10 == 0:
                print(f"Baked frame {frame}/{end_frame} - {len(triangles)} triangles")

    print(f"Successfully baked {num_frames} frames to {out_path}")

if __name__ == "__main__":
    bake_to_a3d()
