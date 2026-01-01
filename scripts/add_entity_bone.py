import os
import sys
import argparse
import numpy as np
from scipy.spatial.transform import Rotation as R
import copy

sys.setrecursionlimit(2000)

class BVHNode:
    def __init__(self, name, parent=None):
        self.name = name
        self.parent = parent
        self.children = []
        self.offset = np.zeros(3)
        self.channels = []
        self.channel_indices = []

    def add_child(self, child):
        self.children.append(child)
        child.parent = self

class BVH:
    def __init__(self):
        self.root = None
        self.nodes = {}
        self.frame_time = 0.0
        self.frames = 0
        self.data = None
        self.channel_names = []

    def load(self, filepath):
        with open(filepath, 'r') as f:
            content = f.read()
        
        lines = content.split('\n')
        iterator = iter(lines)
        
        try:
            line = next(iterator).strip()
            if line != "HIERARCHY":
                raise ValueError("Invalid BVH file: Missing HIERARCHY")
            
            line = next(iterator).strip()
            if not line.startswith("ROOT"):
                raise ValueError("Invalid BVH file: Missing ROOT")
            
            root_name = line.split()[1]
            self.root = BVHNode(root_name)
            self.nodes[root_name] = self.root
            
            stack = [self.root]
            channel_counter = 0
            
            while stack:
                node = stack[-1]
                try:
                    line = next(iterator).strip()
                except StopIteration:
                    break
                    
                if line == "{":
                    continue
                elif line == "}":
                    stack.pop()
                    continue
                
                parts = line.split()
                if not parts: continue

                if parts[0] == "OFFSET":
                    node.offset = np.array([float(parts[1]), float(parts[2]), float(parts[3])])
                elif parts[0] == "CHANNELS":
                    num_channels = int(parts[1])
                    node.channels = parts[2:]
                    node.channel_indices = list(range(channel_counter, channel_counter + num_channels))
                    channel_counter += num_channels
                    for ch in node.channels:
                        self.channel_names.append(f"{node.name}_{ch}")
                elif parts[0] == "JOINT":
                    child_name = parts[1]
                    child = BVHNode(child_name, node)
                    node.add_child(child)
                    self.nodes[child_name] = child
                    stack.append(child)
                elif parts[0] == "End": 
                    child_name = f"{node.name}_EndSite"
                    child = BVHNode(child_name, node)
                    node.add_child(child)
                    self.nodes[child_name] = child
                    stack.append(child)
                elif parts[0] == "MOTION":
                    break

            while not line.startswith("MOTION"):
                try:
                    line = next(iterator).strip()
                except StopIteration:
                    break
            
            line = next(iterator).strip()
            if line.startswith("Frames:"):
                self.frames = int(line.split()[1])
            
            line = next(iterator).strip()
            if line.startswith("Frame Time:"):
                self.frame_time = float(line.split()[2])
                
            data_lines = []
            for line in iterator:
                line = line.strip()
                if line:
                    data_lines.append([float(x) for x in line.split()])
            
            self.data = np.array(data_lines)
            if self.data.shape[0] != self.frames:
                print(f"Warning: Frame count mismatch. Header: {self.frames}, Data: {self.data.shape[0]}")
                self.frames = self.data.shape[0]
                
        except StopIteration:
            pass

    def save(self, filepath):
        with open(filepath, 'w') as f:
            f.write("HIERARCHY\n")
            self._write_node(f, self.root, 0)
            f.write("MOTION\n")
            f.write(f"Frames: {self.frames}\n")
            f.write(f"Frame Time: {self.frame_time}\n")
            for frame in self.data:
                f.write(" ".join(f"{x:.6f}" for x in frame) + "\n")

    def _write_node(self, f, node, indent):
        tab = "\t" * indent
        if node.parent is None:
            f.write(f"{tab}ROOT {node.name}\n")
        elif node.name.endswith("_EndSite"):
            f.write(f"{tab}End Site\n")
        else:
            f.write(f"{tab}JOINT {node.name}\n")
        
        f.write(f"{tab}{{\n")
        f.write(f"{tab}\tOFFSET {node.offset[0]:.6f} {node.offset[1]:.6f} {node.offset[2]:.6f}\n")
        
        if not node.name.endswith("_EndSite"):
            f.write(f"{tab}\tCHANNELS {len(node.channels)} {' '.join(node.channels)}\n")
        
        for child in node.children:
            self._write_node(f, child, indent + 1)
            
        f.write(f"{tab}}}\n")

    def get_global_transforms(self, frame_idx):
        transforms = {}
        
        def compute_transform(node, parent_transform):
            # Local transform
            # Check if node has position channels (like Root or Simulation)
            has_pos_channels = any("position" in ch for ch in node.channels)
            
            if has_pos_channels:
                # Ignore OFFSET, use channels
                tx, ty, tz = 0.0, 0.0, 0.0
                for i, ch in enumerate(node.channels):
                    val = self.data[frame_idx, node.channel_indices[i]]
                    if ch == "Xposition": tx = val
                    elif ch == "Yposition": ty = val
                    elif ch == "Zposition": tz = val
            else:
                # Use OFFSET
                tx, ty, tz = node.offset
            
            local_pos = np.array([tx, ty, tz])

            # Rotation
            eulers = []
            order = ""
            for i, ch in enumerate(node.channels):
                if "rotation" in ch:
                    val = self.data[frame_idx, node.channel_indices[i]]
                    if ch == "Xrotation": order += "x"; eulers.append(val)
                    elif ch == "Yrotation": order += "y"; eulers.append(val)
                    elif ch == "Zrotation": order += "z"; eulers.append(val)
            
            if not order:
                local_rot = R.identity()
            else:
                local_rot = R.from_euler(order, eulers, degrees=True)
            
            if parent_transform is None:
                global_rot = local_rot
                global_pos = local_pos
            else:
                parent_pos, parent_rot = parent_transform
                global_rot = parent_rot * local_rot
                global_pos = parent_pos + parent_rot.apply(local_pos)
            
            transforms[node.name] = (global_pos, global_rot)
            
            for child in node.children:
                compute_transform(child, (global_pos, global_rot))
        
        compute_transform(self.root, None)
        return transforms

    def add_simulation_bone(self):
        hips = self.root
        spine2 = None
        
        # Find Spine2
        queue = [hips]
        while queue:
            n = queue.pop(0)
            if "Spine2" in n.name or "spine2" in n.name:
                spine2 = n
                break
            queue.extend(n.children)
        
        if spine2 is None:
            print("Warning: Spine2 not found. Searching for Spine1...")
            queue = [hips]
            while queue:
                n = queue.pop(0)
                if "Spine1" in n.name or "spine1" in n.name:
                    spine2 = n
                    break
                queue.extend(n.children)
            
            if spine2 is None:
                 print("Warning: Spine1 not found. Using Hips.")
                 spine2 = hips

        print(f"Using {spine2.name} for Simulation reference.")

        # Create Simulation Node
        # Must use 6 channels to be compatible with bvh11.cpp if it expects 6 for root/translation
        sim_node = BVHNode("Simulation")
        sim_node.channels = ["Xposition", "Yposition", "Zposition", "Zrotation", "Xrotation", "Yrotation"]
        sim_node.offset = np.zeros(3)
        
        # Store old BVH for calculation
        old_bvh = copy.deepcopy(self)
        
        # Update Hierarchy
        old_root = self.root
        self.root = sim_node
        sim_node.add_child(old_root)
        self.nodes["Simulation"] = sim_node
        
        # Re-index channels
        sim_node.channel_indices = [0, 1, 2, 3, 4, 5]
        current_idx = 6
        
        def update_indices(node):
            nonlocal current_idx
            if node == sim_node: return
            node.channel_indices = list(range(current_idx, current_idx + len(node.channels)))
            current_idx += len(node.channels)
            for child in node.children:
                update_indices(child)
                
        update_indices(old_root)
        
        total_channels = current_idx
        new_motion_data = np.zeros((self.frames, total_channels))
        
        for f in range(self.frames):
            transforms = old_bvh.get_global_transforms(f)
            
            P_hips_world, R_hips_world = transforms[old_root.name]
            P_spine2_world, _ = transforms[spine2.name]
            
            # P_sim
            P_sim_world = np.array([P_spine2_world[0], 0.0, P_spine2_world[2]])
            
            # R_sim
            v_fwd_local = np.array([0.0, 1.0, 0.0]) # Y is forward
            D_hips = R_hips_world.apply(v_fwd_local)
            D_sim = np.array([D_hips[0], 0.0, D_hips[2]])
            norm = np.linalg.norm(D_sim)
            if norm < 1e-6: D_sim = np.array([0.0, 0.0, 1.0])
            else: D_sim = D_sim / norm
            
            yaw = np.arctan2(D_sim[0], D_sim[2])
            R_sim_world = R.from_euler('y', yaw, degrees=False)
            
            # Hips Local
            R_hips_local = R_sim_world.inv() * R_hips_world
            P_hips_local = R_sim_world.inv().apply(P_hips_world - P_sim_world)
            
            # Fill Data
            # Sim
            new_motion_data[f, 0] = P_sim_world[0]
            new_motion_data[f, 1] = P_sim_world[1]
            new_motion_data[f, 2] = P_sim_world[2]
            # Rotation: ZXY order. Z=0, X=0, Y=yaw
            new_motion_data[f, 3] = 0.0
            new_motion_data[f, 4] = 0.0
            new_motion_data[f, 5] = np.degrees(yaw)
            
            # Hips
            hips_node = self.nodes[old_root.name]
            
            # Hips Position
            # Since Hips has position channels, bvh11.cpp ignores offset.
            # So we put the full local position into channels.
            channel_pos = P_hips_local
            
            for i, ch in enumerate(hips_node.channels):
                idx = hips_node.channel_indices[i]
                if ch == "Xposition": new_motion_data[f, idx] = channel_pos[0]
                elif ch == "Yposition": new_motion_data[f, idx] = channel_pos[1]
                elif ch == "Zposition": new_motion_data[f, idx] = channel_pos[2]
            
            # Hips Rotation
            euler_order = ""
            euler_indices = []
            for i, ch in enumerate(hips_node.channels):
                if "rotation" in ch:
                    if ch == "Xrotation": euler_order += "x"
                    elif ch == "Yrotation": euler_order += "y"
                    elif ch == "Zrotation": euler_order += "z"
                    euler_indices.append(hips_node.channel_indices[i])
            
            if euler_order:
                eulers = R_hips_local.as_euler(euler_order, degrees=True)
                for i, val in enumerate(eulers):
                    new_motion_data[f, euler_indices[i]] = val
            
            # Other joints
            for name, node in self.nodes.items():
                if name == "Simulation" or name == old_root.name: continue
                if name.endswith("_EndSite"): continue
                
                old_node = old_bvh.nodes[name]
                for i, ch in enumerate(node.channels):
                    new_idx = node.channel_indices[i]
                    old_idx = old_node.channel_indices[i]
                    new_motion_data[f, new_idx] = old_bvh.data[f, old_idx]
                    
        self.data = new_motion_data
        return old_bvh

def verify(old_bvh, new_bvh):
    print("Verifying...")
    frames = old_bvh.frames
    
    max_pos_err = 0.0
    max_rot_err = 0.0
    
    for f in range(frames):
        old_transforms = old_bvh.get_global_transforms(f)
        new_transforms = new_bvh.get_global_transforms(f)
        
        for name in old_transforms:
            if name not in new_transforms:
                continue
                
            p_old, r_old = old_transforms[name]
            p_new, r_new = new_transforms[name]
            
            pos_err = np.linalg.norm(p_old - p_new)
            
            r_diff = r_old * r_new.inv()
            angle = r_diff.magnitude()
            
            max_pos_err = max(max_pos_err, pos_err)
            max_rot_err = max(max_rot_err, angle)
            
            if pos_err > 1e-2 or angle > 1e-2:
                print(f"Frame {f}, Node {name}: Pos Err {pos_err}, Rot Err {angle}")
                # return False
    
    print(f"Max Position Error: {max_pos_err}")
    print(f"Max Rotation Error: {max_rot_err}")
    
    if max_pos_err < 1e-2 and max_rot_err < 1e-2:
        print("Verification PASSED")
        return True
    else:
        print("Verification FAILED")
        return False

def process_file(input_path, output_path):
    print(f"Processing {input_path} -> {output_path}")
    bvh = BVH()
    bvh.load(input_path)
    
    old_bvh = bvh.add_simulation_bone()
    
    if verify(old_bvh, bvh):
        bvh.save(output_path)
        print(f"Saved to {output_path}")
    else:
        print("Skipping save due to verification failure.")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Add Simulation bone to BVH file.')
    parser.add_argument('input_file', type=str, help='Path to input BVH file')
    parser.add_argument('output_file', type=str, nargs='?', help='Path to output BVH file (optional)')
    
    args = parser.parse_args()
    
    input_path = args.input_file
    if args.output_file:
        output_path = args.output_file
    else:
        base, ext = os.path.splitext(input_path)
        output_path = f"{base}_with_entity{ext}"
        
    if os.path.exists(input_path):
        process_file(input_path, output_path)
    else:
        print(f"Error: Input file '{input_path}' not found.")
        sys.exit(1)
