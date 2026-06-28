#!/bin/bash
if [ -z "$2" ] || [ -z "$3" ]; then
	echo "Usage: $0 <old suffix|-> <new id> <new name>"
	echo
	echo "Example: $0 uuid_uk_klatchian_mist klatchian_mist_1 \"Klatchian Mist 1\""
	exit 1
fi
OLD="$1"
NEW="$2"
NAME="$3"
[ "$OLD" = "-" ] && OLD=""
shift 3

function generate_file() {
	echo "{"

	old="switch.${OLD}_switch"
	new="switch.${NEW}_power"
	[ -n "$OLD" ] || old="$new"
	echo "  \"${old}\": [\"${new}\", \"${NAME} Power\"],"

	old="switch.${OLD}_switch_2"
	new="switch.${NEW}_ioniser"
	[ -n "$OLD" ] || old="$new"
	echo "  \"${old}\": [\"${new}\", \"${NAME} Ioniser\"],"

	old="number.${OLD}_number_mode"
	new="number.${NEW}_mode"
	[ -n "$OLD" ] || old="$new"
	echo "  \"${old}\": [\"${new}\", \"${NAME} Mode\"],"

	old="number.${OLD}_number_fan_speed"
	new="number.${NEW}_fan_speed"
	[ -n "$OLD" ] || old="$new"
	echo "  \"${old}\": [\"${new}\", \"${NAME} Fan Speed\"],"

	old="number.${OLD}_number_humidity_setpoint"
	new="number.${NEW}_humidity"
	[ -n "$OLD" ] || old="$new"
	echo "  \"${old}\": [\"${new}\", \"${NAME} Humidity Setpoint\"],"

	old="switch.${OLD}_switch_2"
	new="switch.${NEW}_ioniser"
	[ -n "$OLD" ] || old="$new"
	echo "  \"${old}\": [\"${new}\", \"${NAME} Ioniser\"],"

	old="binary_sensor.${OLD}_binaryinput"
	new="binary_sensor.${NEW}_auto_defrost"
	[ -n "$OLD" ] || old="$new"
	echo "  \"${old}\": [\"${new}\", \"${NAME} Auto Defrost\"],"

	old="binary_sensor.${OLD}_binaryinput_2"
	new="binary_sensor.${NEW}_bucket_full"
	[ -n "$OLD" ] || old="$new"
	echo "  \"${old}\": [\"${new}\", \"${NAME} Bucket Full\"],"

	old="button.${OLD}_identify"
	new="button.${NEW}_identify"
	[ -n "$OLD" ] || old="$new"
	echo "  \"${old}\": [\"${new}\", \"${NAME} Identify\"],"

	old="sensor.${OLD}_analoginput"
	new="sensor.${NEW}_uptime"
	[ -n "$OLD" ] || old="$new"
	echo "  \"${old}\": [\"${new}\", \"${NAME} Uptime (days)\"],"

	old="sensor.${OLD}_analoginput_2"
	new="sensor.${NEW}_humidity"
	[ -n "$OLD" ] || old="$new"
	echo "  \"${old}\": [\"${new}\", \"${NAME} Humidity Reading\"],"

	old="sensor.${OLD}_analoginput_3"
	new="sensor.${NEW}_temperature"
	[ -n "$OLD" ] || old="$new"
	echo "  \"${old}\": [\"${new}\", \"${NAME} Temperature\"],"

	old="sensor.${OLD}_analoginput_4"
	new="sensor.${NEW}_connected_time"
	[ -n "$OLD" ] || old="$new"
	echo "  \"${old}\": [\"${new}\", \"${NAME} Connected time (days)\"],"

	old="sensor.${OLD}_analoginput_5"
	new="sensor.${NEW}_uplink_address"
	[ -n "$OLD" ] || old="$new"
	echo "  \"${old}\": [\"${new}\", \"${NAME} Uplink address\"],"

	old="sensor.${OLD}_analoginput_6"
	new="sensor.${NEW}_uplink_rssi"
	[ -n "$OLD" ] || old="$new"
	echo "  \"${old}\": [\"${new}\", \"${NAME} Uplink RSSI\"],"

	old="sensor.${OLD}_lqi"
	new="sensor.${NEW}_lqi"
	[ -n "$OLD" ] || old="$new"
	echo "  \"${old}\": [\"${new}\", \"${NAME} LQI\"],"

	old="sensor.${OLD}_rssi"
	new="sensor.${NEW}_rssi"
	[ -n "$OLD" ] || old="$new"
	echo "  \"${old}\": [\"${new}\", \"${NAME} RSSI\"]"

	echo "}"
}

pipenv run ./homeassistant-entity-renamer.py --file <(generate_file)
