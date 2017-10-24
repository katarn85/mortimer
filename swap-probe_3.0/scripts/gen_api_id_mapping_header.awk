# This script generates api_id_mapping header from api list

BEGIN {
	print "/*"
	print " * this file genereted by <swap-probe> project with cmd <make headers>"
	print " */"
	print
	print "#ifndef __API_ID_MAP__"
	print "#define __API_ID_MAP__"
	print
	print "#ifdef __cplusplus"
	print "extern \"C\"{"
	print "#endif"
	print
	api_id = 1
	macro_prefix = "API_ID_"
} {
	if ( $0 == "" ) {
		print
	} else {
		orig = $0
		def = orig
		split(def, splited, "###")
		def = splited[1]
		gsub(/[,:()*&~\[\] ]/, "_", def)
		def = macro_prefix def
		printf "#define %-135s %d // %s\n", def, api_id, orig
		api_id = api_id + 1
	}
}
END {
	print
	print "#ifdef __cplusplus"
	print "}"
	print "#endif"
	print
	print "#endif /* __API_ID_MAP__ */"
}
