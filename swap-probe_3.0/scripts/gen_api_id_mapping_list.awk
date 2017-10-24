# This script generates api_id_mapping text file from api list

BEGIN {
    api_id = 1
} {
	if ( $0 != "" ) {
		api_name = $0
		split(api_name, splited, "###")
		if (splited[2] != ""){
			api_name = splited[2]
		}
		printf "%d %s\n", api_id, api_name
		api_id = api_id + 1
	}
}
