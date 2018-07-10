#!/bin/bash

decimal_to_hex() {
	local integers=${1}
	local hex=""
	local len=""
	local swap_hex=""

	hex=$(echo "obase=16; ${integers}" | bc)

	len=$(echo ${#hex})

	echo "Decimal = ${integers}"
	echo "Hex = ${hex}"
	echo "Len = ${len}"

	if [ $[$len%2] -eq 0 ]; then
		for ((i=$((len - 2)); i>=0; i=$((i - 2))))
		do
			swap_hex="${swap_hex}${hex:${i}:2}"
		done
	else
		if [ X"${len}" == X"1" ]; then
			swap_hex="${swap_hex}${hex:0:1}"
		else
			for ((i=$((len - 2)); i>=-1; i=((i - 2))))
			do
				echo "i = ${i}"
				if [ X"${i}" == X"-1" ]; then
					swap_hex="${swap_hex}${hex:0:1}"
				else
					swap_hex="${swap_hex}${hex:${i}:2}"
				fi
			done
		fi
	fi

	echo "swap_hex = ${swap_hex}"
}

decimal_to_hex ${1}
