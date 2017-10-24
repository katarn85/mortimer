# This script generates api_id decode list header from api list
#
# use:
# cat api_names.txt | awk -f ./gen_api_id_mapping_header_list.awk > sample.h
# where api_names.txt is function name list file

BEGIN {
	api_id = 1
	print "/*"
	print " * this file genereted by <swap-probe> project with cmd <make headers>"
	print " */"
	print
	print "#ifdef __cplusplus"
	print "extern \"C\"{"
	print "#endif"
	print
	print "const char *api_id_list[] = {"
} {
	if ( $0 == "" ) {
		print
	} else {
		orig = $0
		def = orig
		split(def, splited, "###")
		if (splited[2] != ""){
			def = splited[2]
		}
		def = def "\","
		printf "\"%-135s//%d %s\n", def, api_id, orig
		api_id = api_id + 1
	}
}

END {
	print
	print "#ifdef __cplusplus"
	print "}"
	print "#endif"
	print
	print "};"
}
