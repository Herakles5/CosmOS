import os
import glob
import subprocess
import shutil

downloads_dir = "/root/Downloads/"
avatar_dir = "/root/coding/MeinOS/avatar/"
baker_script = "/root/coding/MeinOS/avatar_baker.py"

def main():
    zips = sorted(glob.glob(os.path.join(downloads_dir, "*.zip")))
    
    # Filter zips to only include likely model files
    baked_models = []
    
    for i, zip_file in enumerate(zips):
        if "League of Legends" in zip_file:
            continue
            
        expected_out = os.path.join(avatar_dir, f"model_{len(baked_models)}.a3d")
        if os.path.exists(expected_out):
            print(f"Skipping {os.path.basename(zip_file)} (already baked as {expected_out})")
            baked_models.append(os.path.basename(expected_out))
            continue
            
        print(f"\n--- Processing {os.path.basename(zip_file)} ---")
        
        # 1. Clear avatar dir except existing .a3d files
        for filename in os.listdir(avatar_dir):
            if not filename.endswith(".a3d"):
                file_path = os.path.join(avatar_dir, filename)
                try:
                    if os.path.isfile(file_path) or os.path.islink(file_path):
                        os.unlink(file_path)
                    elif os.path.isdir(file_path):
                        shutil.rmtree(file_path)
                except Exception as e:
                    print(f'Failed to delete {file_path}. Reason: {e}')
                    
        # 2. Extract zip
        subprocess.run(["unzip", "-q", "-o", zip_file, "-d", avatar_dir])
        
        # 3. Bake
        result = subprocess.run(["blender", "-b", "-P", baker_script], capture_output=True, text=True)
        
        # 4. Check if avatar.a3d exists
        out_a3d = os.path.join(avatar_dir, "avatar.a3d")
        if os.path.exists(out_a3d):
            new_name = f"model_{len(baked_models)}.a3d"
            new_path = os.path.join(avatar_dir, new_name)
            os.rename(out_a3d, new_path)
            baked_models.append(new_name)
            print(f"Successfully baked -> {new_name}")
        else:
            print("Baking failed. Blender output:")
            print(result.stdout)
            print(result.stderr)
            
    print(f"\nCompleted! Baked {len(baked_models)} models: {baked_models}")
    
    # Save a configuration file so the C++ engine knows how many models there are!
    with open(os.path.join(avatar_dir, "models.txt"), "w") as f:
        f.write(str(len(baked_models)) + "\n")
        for m in baked_models:
            f.write(m + "\n")

if __name__ == "__main__":
    main()
