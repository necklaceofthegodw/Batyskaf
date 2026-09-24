"""Create a separate ~500 triangle copy of the original fish; keep source intact."""
import bpy, os, json, math
from mathutils import Vector
ROOT=os.path.dirname(os.path.abspath(__file__))
OUT=os.path.join(ROOT,'Low500'); os.makedirs(OUT,exist_ok=True)
bpy.ops.wm.open_mainfile(filepath=os.path.join(ROOT,'Pufferfish_Spiny.blend'))
fish=bpy.data.objects['SM_Pufferfish_Spiny']
bpy.ops.object.select_all(action='DESELECT'); fish.select_set(True)
bpy.context.view_layer.objects.active=fish
material=fish.data.materials[0]
bpy.ops.object.mode_set(mode='EDIT'); bpy.ops.mesh.select_all(action='SELECT')
bpy.ops.mesh.separate(type='LOOSE'); bpy.ops.object.mode_set(mode='OBJECT')
pieces=list(bpy.context.selected_objects)
for o in pieces:
 bpy.context.view_layer.objects.active=o
 nv=len(o.data.vertices)
 if nv==11: # Original five-sided, two-ring spine: rebuild as a triangular cone.
  vs=[v.co.copy() for v in o.data.vertices]
  vs.sort(key=lambda v:v.length)
  base=sum(vs[:5],Vector())/5; tip=vs[-1]
  normal=(tip-base).normalized(); q=normal.to_track_quat('Z','Y')
  radius=sum((v-base).length for v in vs[:5])/5
  verts=[base+q@Vector((math.cos(j*2*math.pi/3)*radius,math.sin(j*2*math.pi/3)*radius,0)) for j in range(3)]+[tip]
  me=bpy.data.meshes.new('Spine_Triangular'); me.from_pydata(verts,[],[(0,1,3),(1,2,3),(2,0,3)]); me.update()
  # Base is buried in the opaque body, so its hidden cap can be omitted.
  o.data=me; me.materials.append(material); uv=me.uv_layers.new(name='UV_Palette')
  for p in me.polygons:
   for li in p.loop_indices: uv.data[li].uv=((10%8+.5)/8,(10//8+.5)/4)
 else:
  o.data.calc_loop_triangles(); tris=len(o.data.loop_triangles)
  target=112 if nv==162 else 12 if nv==42 else 4 if nv==12 and tris==20 else 16 if nv==48 else max(6,int(tris*.6))
  mod=o.modifiers.new('Feature-aware reduction','DECIMATE'); mod.ratio=min(1,target/tris); mod.use_collapse_triangulate=True
  bpy.ops.object.modifier_apply(modifier=mod.name)
bpy.context.view_layer.objects.active=pieces[0]; bpy.ops.object.join()
fish=bpy.context.object; fish.name='SM_Pufferfish_Spiny_500'
fish.data.calc_loop_triangles()
if len(fish.data.loop_triangles)>500:
 mod=fish.modifiers.new('Final triangle budget','DECIMATE'); mod.ratio=500/len(fish.data.loop_triangles); mod.use_collapse_triangulate=True
 bpy.ops.object.modifier_apply(modifier=mod.name)
fish.data.calc_loop_triangles()
stats={'triangles':len(fish.data.loop_triangles),'vertices':len(fish.data.vertices),'materials':len(fish.data.materials),'dimensions_m':list(fish.dimensions),'source':'../Pufferfish_Spiny.blend'}
with open(os.path.join(OUT,'mesh_stats.json'),'w') as f: json.dump(stats,f,indent=2)
bpy.ops.export_scene.fbx(filepath=os.path.join(OUT,'SM_Pufferfish_Spiny_500.fbx'),use_selection=True,object_types={'MESH'},apply_unit_scale=True,axis_forward='-Y',axis_up='Z',use_mesh_modifiers=True,mesh_smooth_type='FACE',path_mode='COPY',embed_textures=True,add_leaf_bones=False)
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(OUT,'Pufferfish_Spiny_500.blend'))
bpy.context.scene.render.filepath=os.path.join(OUT,'Pufferfish_500_preview.png')
bpy.ops.render.render(write_still=True)
print('LOW500_COMPLETE',json.dumps(stats))
