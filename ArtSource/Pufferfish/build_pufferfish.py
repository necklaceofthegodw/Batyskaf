"""Rebuild the original Batyskaf spiny pufferfish in Blender 2.93+ (no add-ons)."""
import bpy, math, random, json, os
from mathutils import Vector

OUT = os.path.dirname(os.path.abspath(__file__))
random.seed(24)
bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)
scene=bpy.context.scene
scene.unit_settings.system='METRIC'
scene.unit_settings.scale_length=1.0
# Palette uses the ochre, ivory and near-black language of the project's LowPolyFish.
COLORS=['B88936','C99C45','D9AF55','E6BF6C','A97B30','F1DEAC','FAEBC9','E5C991',
        '634326','78532A','FBEAC0','D6AD62','DC963F','F0B857','342F26','101E24',
        'FFF7DE','98662F','B67C35','EBCB8D','CCAA68','E6D4A5','8B642E','D5B978']
def rgba(h): return tuple(int(h[i:i+2],16)/255 for i in (0,2,4))+(1,)
atlas=bpy.data.images.new('T_Pufferfish_Palette',width=256,height=256,alpha=False)
pix=[]
for y in range(256):
 for x in range(256): pix.extend(rgba(COLORS[min(23,(y//64)*8+x//32)]))
atlas.pixels=pix
atlas.filepath_raw=os.path.join(OUT,'T_Pufferfish_Palette.png')
atlas.file_format='PNG'; atlas.save()
mat=bpy.data.materials.new('M_Pufferfish_Palette'); mat.use_nodes=True
shader=mat.node_tree.nodes.get('Principled BSDF')
shader.inputs['Roughness'].default_value=.83
tex=mat.node_tree.nodes.new('ShaderNodeTexImage'); tex.image=atlas; tex.interpolation='Closest'
mat.node_tree.links.new(tex.outputs['Color'],shader.inputs['Base Color'])
parts=[]
def finish(obj,name,idx):
 obj.name=name
 bpy.context.view_layer.objects.active=obj
 bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
 obj.data.materials.clear(); obj.data.materials.append(mat)
 uv=obj.data.uv_layers.active or obj.data.uv_layers.new(name='UV_Palette')
 for p in obj.data.polygons:
  c=idx(p,obj) if callable(idx) else idx
  for li in p.loop_indices: uv.data[li].uv=((c%8+.5)/8,(c//8+.5)/4)
  p.use_smooth=False
 parts.append(obj)
 return obj
def ico(name,loc,scale,idx,sub=2,normal=None):
 bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=sub,radius=1,location=loc)
 o=bpy.context.object; o.scale=scale
 if normal is not None: o.rotation_euler=Vector(normal).to_track_quat('Z','Y').to_euler()
 return finish(o,name,idx)
spots=[]
for i in range(32):
 z=random.uniform(-.25,.96); a=random.uniform(0,2*math.pi)
 d=Vector((math.sqrt(1-z*z)*math.cos(a),math.sqrt(1-z*z)*math.sin(a),z))
 if d.x<.72: spots.append(d)
def body_color(p,o):
 n=p.center.normalized()
 if n.z<-.27: return random.choice([5,5,6,6,21])
 if n.z<-.13: return random.choice([5,7,19])
 if any(n.dot(s)>.993 for s in spots): return random.choice([8,9,22])
 return random.choice([0,1,1,2,2,3,4]) if n.z>.35 else random.choice([1,2,3,7])
body=ico('Body',(0,0,0),(.51,.47,.49),body_color,3)
# Eyes deliberately project from the face and remain legible at gameplay distance.
for side in [-1,1]:
 n=Vector((.83,side*.54,.13)).normalized()
 anchor=Vector((.392,side*.274,.155))
 ico('EyeSocket',anchor,(.132,.146,.07),7,2,n)
 ico('EyeAmber',anchor+n*.047,(.103,.115,.045),11,2,n)
 ico('EyePupil',anchor+n*.084,(.074,.086,.035),15,2,n)
 tangent=Vector((0,0,1))
 ico('EyeGlint',anchor+n*.115+tangent*.030+Vector((0,side*-.018,0)),(.019,.019,.009),16,1,n)
# Small pursed circular mouth, characteristic of a pufferfish.
bpy.ops.mesh.primitive_torus_add(major_segments=12,minor_segments=4,location=(.506,0,-.089),major_radius=.058,minor_radius=.016,rotation=(0,math.pi/2,0))
o=bpy.context.object; o.scale=(.8,1,1); finish(o,'MouthLip',17)
ico('MouthOpening',(.507,0,-.089),(.012,.043,.036),14,2)
# Golden-angle distribution avoids the mechanical appearance of latitude rows.
count=0
for i in range(108):
 z=1-2*(i+.5)/108; a=i*math.pi*(3-math.sqrt(5))
 d=Vector((math.sqrt(1-z*z)*math.cos(a),math.sqrt(1-z*z)*math.sin(a),z))
 if d.x>.64 or d.x<-.94: continue
 if abs(d.y)>.88 and -.3<d.z<.13: continue
 base=Vector((d.x*.501,d.y*.461,d.z*.481))
 length=random.uniform(.13,.22)*(1.08 if d.z>.25 else .85)
 radius=random.uniform(.027,.043)
 q=d.to_track_quat('Z','Y')
 verts=[]
 for h,r in [(0,radius),(.36*length,radius*.65)]:
  for j in range(5):
   v=q@Vector((math.cos(j*2*math.pi/5)*r,math.sin(j*2*math.pi/5)*r,h))
   verts.append(base+v)
 verts.append(base+d*length)
 faces=[]
 for j in range(5):
  k=(j+1)%5; faces.extend([(j,k,k+5,j+5),(j+5,k+5,10)])
 faces.append((4,3,2,1,0))
 me=bpy.data.meshes.new('Spine'); me.from_pydata(verts,[],faces); me.update()
 o=bpy.data.objects.new('Spine',me); bpy.context.collection.objects.link(o)
 finish(o,'Spine_%02d'%count,lambda p,o: 10 if p.index%2 else 11)
 count+=1
# Thick fan fins: enclosed meshes, no alpha cards or double-sided material needed.
def fan(name,root,rim,thickness=.012):
 # Fan lies in XZ for tail, XY-ish for side fins; thickness supplied as a vector.
 t=Vector(thickness) if isinstance(thickness,tuple) else Vector((0,thickness,0))
 pts=[Vector(root)]+[Vector(v) for v in rim]; N=len(pts)
 vs=[p+t for p in pts]+[p-t for p in pts]; fs=[]
 for j in range(1,N-1): fs.extend([(0,j,j+1),(N,N+j+1,N+j)])
 border=list(range(N))
 for j in range(N):
  k=(j+1)%N; fs.append((j,N+j,N+k,k))
 me=bpy.data.meshes.new(name); me.from_pydata(vs,[],fs); me.update()
 o=bpy.data.objects.new(name,me); bpy.context.collection.objects.link(o)
 finish(o,name,lambda p,o:[12,13,18,13][(p.index//2)%4])
for s in [-1,1]:
 fan('PectoralFin',(-.02,s*.432,-.065),[(.055,s*.60,-.015),(-.055,s*.755,-.045),(-.18,s*.76,-.10),(-.28,s*.665,-.155),(-.21,s*.47,-.13)],(0,0,.014))
ico('TailPeduncle',(-.50,0,.005),(.18,.093,.10),1,1)
fan('TailFin',(-.57,0,.00),[(-.85,0,.245),(-.94,0,.195),(-.885,0,.055),(-.91,0,-.06),(-.96,0,-.20),(-.85,0,-.24)])
fan('DorsalFin',(-.28,0,.365),[(-.24,0,.59),(-.40,0,.64),(-.51,0,.46)])
# Single material, single mesh, flat normals; pivot at the center of the body.
bpy.ops.object.select_all(action='DESELECT')
for o in parts: o.select_set(True)
bpy.context.view_layer.objects.active=body; bpy.ops.object.join()
fish=bpy.context.object; fish.name='SM_Pufferfish_Spiny'
scene.cursor.location=(0,0,0); bpy.ops.object.origin_set(type='ORIGIN_CURSOR')
# Joining objects sharing one material should preserve one slot; normalize explicitly.
fish.data.materials.clear(); fish.data.materials.append(mat)
for p in fish.data.polygons: p.material_index=0
bpy.ops.object.transform_apply(location=False,rotation=True,scale=True)
fish.data.calc_loop_triangles()
stats={'triangles':len(fish.data.loop_triangles),'vertices':len(fish.data.vertices),'spines':count,'materials':1,'dimensions_m':list(fish.dimensions),'forward':'+X','rigged':False}
with open(os.path.join(OUT,'mesh_stats.json'),'w') as f: json.dump(stats,f,indent=2)
bpy.ops.export_scene.fbx(filepath=os.path.join(OUT,'SM_Pufferfish_Spiny.fbx'),use_selection=True,object_types={'MESH'},apply_unit_scale=True,axis_forward='-Y',axis_up='Z',use_mesh_modifiers=True,mesh_smooth_type='FACE',use_tspace=False,path_mode='COPY',embed_textures=True,add_leaf_bones=False)
# Presentation studio, excluded from the FBX export.
studio=bpy.data.collections.new('Preview_Studio'); scene.collection.children.link(studio)
def to_studio(o):
 for col in list(o.users_collection): col.objects.unlink(o)
 studio.objects.link(o)
def track(o,target): o.rotation_euler=(Vector(target)-o.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.object.camera_add(location=(2.5,-3.3,1.5)); cam=bpy.context.object; to_studio(cam)
track(cam,(-.09,0,0)); cam.data.type='ORTHO'; cam.data.ortho_scale=2.32; scene.camera=cam
for name,loc,power,size,color in [('Key',(2,-3,4),430,4,(1,.90,.74)),('Fill',(0,3,2),280,3,(.60,.80,1)),('Rim',(-3,0,2),500,2,(.50,.84,.85))]:
 bpy.ops.object.light_add(type='AREA',location=loc); o=bpy.context.object; o.name=name; to_studio(o)
 o.data.energy=power; o.data.size=size; o.data.color=color; track(o,(0,0,0))
scene.world.color=(.10,.10,.10)
scene.render.engine='BLENDER_EEVEE'; scene.eevee.use_gtao=True; scene.eevee.gtao_distance=3
scene.eevee.use_soft_shadows=True; scene.eevee.taa_render_samples=96
scene.render.resolution_x=1200; scene.render.resolution_y=1000; scene.render.resolution_percentage=100
scene.render.image_settings.file_format='PNG'; scene.render.film_transparent=True
scene.view_settings.view_transform='Standard'; scene.view_settings.look='Medium High Contrast'
scene.view_settings.exposure=0; scene.view_settings.gamma=1
bpy.ops.object.select_all(action='DESELECT'); fish.select_set(True); bpy.context.view_layer.objects.active=fish
atlas.pack()
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(OUT,'Pufferfish_Spiny.blend'))
scene.render.filepath=os.path.join(OUT,'Pufferfish_preview.png'); bpy.ops.render.render(write_still=True)
cam.location=(2.8,0,.65); track(cam,(0,0,0)); scene.render.filepath=os.path.join(OUT,'Pufferfish_front.png'); bpy.ops.render.render(write_still=True)
print('PUFFERFISH_COMPLETE',json.dumps(stats))
