import os
import glob
import subprocess
import shutil

downloads_dir = "/root/Downloads/"
meinos_opt = "/opt/meinos/"

baker_script = "/root/coding/MeinOS/avatar_baker_glb.py"

def main():
    zips = sorted(glob.glob(os.path.join(downloads_dir, "*.zip")))
    
    converted_models = []
    
    # We always have model_0.glb as the Fox for now
    # Wait, the user might want to replace it. Let's just convert starting from model_1, 
    # or starting from model_0. Let's start from model_1 so Fox is always 0.
    
    current_idx = 1
    
    for zip_file in zips:
        if "League of Legends" in zip_file:
            continue
            
        expected_out = os.path.join(meinos_opt, f"model_{current_idx}.glb")
        if os.path.exists(expected_out):
            print(f"Skipping {os.path.basename(zip_file)} (already converted as {expected_out})")
            converted_models.append(os.path.basename(expected_out))
            current_idx += 1
            continue
            
        print(f"\n--- Processing {os.path.basename(zip_file)} ---")
        
        tmp_dir = "/tmp/meinos_avatar_convert/"
        if os.path.exists(tmp_dir):
            shutil.rmtree(tmp_dir)
        os.makedirs(tmp_dir)
        
        # 2. Extract zip
        subprocess.run(["unzip", "-q", "-o", zip_file, "-d", tmp_dir])
        
        # 3. Bake using blender
        # We pass the input dir and output path via env vars
        os.environ["AVATAR_TMP_DIR"] = tmp_dir
        os.environ["AVATAR_OUT_GLB"] = expected_out
        
        result = subprocess.run(["blender", "-b", "-P", baker_script], capture_output=True, text=True)
        
        if os.path.exists(expected_out):
            converted_models.append(os.path.basename(expected_out))
            print(f"Successfully converted -> {expected_out}")
            current_idx += 1
        else:
            print("Conversion failed. Blender output:")
            print(result.stdout)
            print(result.stderr)
            
    print(f"\nCompleted! Converted {len(converted_models)} models: {converted_models}")
    
    # Save a configuration file so the C++ engine knows how many models there are!
    # Including model_0 (fox)
    with open(os.path.join(meinos_opt, "models.txt"), "w") as f:
        f.write(str(len(converted_models) + 1) + "\n")
        f.write("model_0.glb\n")
        for m in converted_models:
            f.write(m + "\n")

if __name__ == "__main__":
    main()
