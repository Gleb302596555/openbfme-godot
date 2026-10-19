extends RefCounted
## Generated browser metadata, isolated from retail MapCache.ini and invalidated by mounted archive content/order.
const VERSION = 6
var fs: RefCounted
var scanner: RefCounted
var directory := ''
var generated := 0
var cache_hits := 0

func setup(filesystem: RefCounted, cache_root := 'user://map-definitions'):
 fs=filesystem
 directory=cache_root.path_join('v%d-'%VERSION+str(fs.content_signature()))

func _path(entry: Dictionary) -> String:
 return directory.path_join(str(entry.key).sha256_text()+'.json')

func cached(entry: Dictionary) -> Dictionary:
 var file=FileAccess.open(_path(entry),FileAccess.READ)
 if file==null:return {}
 var result=JSON.parse_string(file.get_as_text())
 if not result is Dictionary:return {}
 if result.get('version',0)!=VERSION or result.get('map','')!=entry.key or not result.get('ok',false):return {}
 if not result.get('capturables') is Array or not result.get('counts') is Dictionary or not result.get('is_fortress') is bool:return {}
 cache_hits+=1
 return result

## Scans use a pristine template world separate from preview overrides. Maps with
## object overrides (including changes to existing templates) need a full scan.
## allow_full false defers those maps until selected, avoiding background INI loads.
func generate(entry: Dictionary, shared: RefCounted = null, allow_full := true) -> Dictionary:
 var name=str(entry.key).replace('\\','/').get_file().get_basename()
 var result:Dictionary=shared.scan_definition(fs,name,{'map_ini':false}) if shared!=null else {'needs_map_ini':true}
 if result.get('needs_map_ini',false):
  if not allow_full:result.map=entry.key;return result
  if scanner==null:scanner=ClassDB.instantiate('MapObjectBuilder')
  result=scanner.scan_definition(fs,name)
 generated+=1
 result.map=entry.key;result.version=VERSION
 if not result.get('ok',false):return result
 var status=DirAccess.make_dir_recursive_absolute(directory)
 if status!=OK:result.cache_error='Cannot create map definition cache: %s'%error_string(status);return result
 var path=_path(entry)
 var file=FileAccess.open(path+'.tmp',FileAccess.WRITE)
 if file==null:result.cache_error='Cannot save map definition: %s'%error_string(FileAccess.get_open_error());return result
 file.store_string(JSON.stringify(result));file.close()
 status=DirAccess.rename_absolute(path+'.tmp',path)
 if status!=OK:result.cache_error='Cannot finish map definition: %s'%error_string(status)
 return result
