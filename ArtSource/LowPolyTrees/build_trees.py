import bpy, math, random, os
from mathutils import Vector
OUT=os.path.dirname(os.path.abspath(__file__))
bpy.ops.object.select_all(action='SELECT'); bpy.ops.object.delete(use_global=False)
bpy.context.scene.unit_settings.system='METRIC'
colors=[(0.17,.32,.20,1),(.24,.43,.23,1),(.34,.51,.26,1),(.45,.59,.30,1),(.25,.15,.08,1),(.36,.23,.12,1),(.54,.38,.19,1),(.28,.42,.32,1)]
im=bpy.data.images.new('T_TreePalette',width=128,height=16)
im.pixels=[v for y in range(16) for x in range(128) for v in colors[x//16]]
im.filepath_raw=os.path.join(OUT,'T_TreePalette.png'); im.file_format='PNG'; im.save()
mat=bpy.data.materials.new('M_LowPolyTree'); mat.use_nodes=True
bs=mat.node_tree.nodes.get('Principled BSDF'); bs.inputs['Roughness'].default_value=.95
tx=mat.node_tree.nodes.new('ShaderNodeTexImage'); tx.image=im; tx.interpolation='Closest'
mat.node_tree.links.new(tx.outputs['Color'],bs.inputs['Base Color'])
def color(o,ids):
 o.data.materials.append(mat); uv=o.data.uv_layers.active or o.data.uv_layers.new()
 for p in o.data.polygons:
  idx=random.choice(ids)
  for l in p.loop_indices: uv.data[l].uv=((idx+.5)/8,.5)
  p.use_smooth=False
 return o
def branch(a,b,r1,r2):
 d=Vector(b)-Vector(a)
 bpy.ops.mesh.primitive_cone_add(vertices=6,radius1=r1,radius2=r2,depth=d.length,location=(Vector(a)+Vector(b))/2)
 o=bpy.context.object; o.rotation_euler=d.to_track_quat('Z','Y').to_euler(); return color(o,[4,5,6])
for variant in range(3):
 random.seed(81+variant); parts=[]
 parts.append(branch((0,0,-.10),(.15,-.10,3.9),.30,.12))
 for j in range(3):
  a=j*2.094+variant*.8; end=(math.cos(a)*1.18,math.sin(a)*1.18,3.5+j*.23)
  parts.append(branch((.08,0,2.2),end,.14,.04))
 crowns=[((.1,0,4.45),(1.45,1.4,1.9))]
 for j in range(3):
  a=j*2.094+variant*.8
  crowns.append(((math.cos(a)*1.12,math.sin(a)*1.12,3.6+j*.25),(1.3,1.25,1.4)))
 for loc,scale in crowns:
  bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=2 if variant==1 else 1,radius=1,location=loc)
  o=bpy.context.object; o.scale=tuple(s*(1+.08*variant) for s in scale); o.rotation_euler=(.12,.2,random.random()*3)
  parts.append(color(o,[0,1,2] if variant==0 else [1,2,3] if variant==1 else [0,1,7]))
 bpy.ops.object.select_all(action='DESELECT')
 for o in parts:o.select_set(True)
 bpy.context.view_layer.objects.active=parts[0]; bpy.ops.object.join()
 o=bpy.context.object; o.name=['SM_Tree_Round','SM_Tree_Lime','SM_Tree_Sage'][variant]
 bpy.context.scene.cursor.location=(0,0,0); bpy.ops.object.origin_set(type='ORIGIN_CURSOR'); bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
 o.data.materials.clear(); o.data.materials.append(mat)
 for p in o.data.polygons:p.material_index=0
 bpy.ops.export_scene.fbx(filepath=os.path.join(OUT,o.name+'.fbx'),use_selection=True,object_types={'MESH'},axis_forward='-Y',axis_up='Z',apply_unit_scale=True,path_mode='COPY',embed_textures=True,mesh_smooth_type='FACE')
 o.hide_set(True)
im.pack(); bpy.ops.wm.save_as_mainfile(filepath=os.path.join(OUT,'LowPolyTrees.blend'))
