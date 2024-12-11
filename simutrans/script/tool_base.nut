// stubs for functions needed by scripted tools
function init(player)
{
	return true
}

function exit(player)
{
	return true
}
// one-click tools
function work(player, pos)
{
	return null
}

// two-click tools
function do_work(player, start, end)
{
	return null
}

function mark_tiles(player, start, end)
{
}

/**
 * Can the tool start/end on pos? If it is the second click, start is the position of the first click
 * 0 = no
 * 1 = This tool can work on this tile (with single click)
 * 2 = On this tile can dragging start/end
 * 3 = Both (1 and 2)
 *
 */
function is_valid_pos(player, start, pos)
{
	return 2
}

// dummy auxiliary function, will be regularly called
function step()
{
	// do not implement
}

// check whether all functions support the flags parameter
function correct_missing_flags_argument()
{
	if (work.getinfos().parameters.len() == 3) {
		work_old <- work
		work = function(player, pos, flags) { return work_old(player, pos) }
	}
	if (do_work.getinfos().parameters.len() == 4) {
		do_work_old <- do_work
		do_work = function(player, start, end, flags) { return do_work_old(player, start, end) }
	}
	if (mark_tiles.getinfos().parameters.len() == 4) {
		mark_tiles_old <- mark_tiles
		mark_tiles = function(player, start, end, flags) { mark_tiles_old(player, start, end) }
	}
}

/**
 * the sender table  will be sent to the scenario script using intern_send_data(str)
 * only plain data is saved: no classes / instances / functions, no cyclic references
 */
sender <- {}

/**
 * writes the sender table to a string
 */
function send_data()
{
	if (typeof(sender) != "table") {
		throw("error during saving: the variable sender is not a table")
	}
	local str = "sender = " + recursive_send(sender, "\t", [ sender ] )

	intern_send_data(str)
	sender.clear()
}

function is_identifier(str)
{
	return (str.toalnum() == str)  &&  (str[0] < '0'  ||  str[0] > '9')
}

function recursive_send(table, indent, table_stack)
{
	local isarray = typeof(table) == "array"
	local str = (isarray ? "[" : "{") + "\n"
	foreach(key, val in table) {
		str += indent
		if (!isarray) {
			if (typeof(key)=="string") {
				if (is_identifier(key)) {
					str += key + " = "
				}
				else {
					str += "[\"" + key + "\"] = "
				}
			}
			else {
				str += "[" + key + "] = "
			}
		}
		while( typeof(val) == "weakref" )
			val = val.ref

		switch( typeof(val) ) {
			case "null":
				str += "null"
				break
			case "integer":
			case "float":
			case "bool":
				str += val
				break
			case "string":
				str += "@\"" + val + "\""
				break
			case "array":
			case "table":
				if (!table_stack.find(val)) {
						table_stack.push( table )
						str += recursive_send(val, indent + "\t", table_stack )
						table_stack.pop()
				}
				else {
					// cyclic reference - good luck with resolving
					str += "null"
				}
				break
			case "generator":
				str += "null"
				break
			case "instance":
			default:
				if ("_save" in val) {
					str += val._save()
					break
				}
				str += "\"unknown(" + typeof(val) + ")\""
		}
		if (str.slice(-1) != "\n") {
			str += ",\n"
		}
		else {
			str = str.slice(0,-1) + ",\n"
		}

	}
	str += indent.slice(0,-1) + (isarray ? "]" : "}") + "\n"
	return str
}
