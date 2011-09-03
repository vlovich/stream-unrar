list(APPEND UNRAR_SRC
	archive.cpp
	arcread.cpp
	cmddata.cpp
	consio.cpp
	crc.cpp
	crypt.cpp
	dll.cpp
	encname.cpp
	errhnd.cpp
	extinfo.cpp
	extract.cpp
	filcreat.cpp
	file.cpp
	filefn.cpp
	filestr.cpp
	find.cpp
	getbits.cpp
	global.cpp
	isnt.cpp
	list.cpp
	match.cpp
	options.cpp
	pathfn.cpp
	rar.cpp
	rarpch.cpp
	rarvm.cpp
	rawread.cpp
	rdwrfn.cpp
	recvol.cpp
	resource.cpp
	rijndael.cpp
	rs.cpp
	savepos.cpp
	scantree.cpp
	sha1.cpp
	smallfn.cpp
	strfn.cpp
	strlist.cpp
	system.cpp
	timefn.cpp
	ulinks.cpp
	unicode.cpp
	unpack.cpp
	volume.cpp
)

# OS2/BeOS, etc unsupported
	#os2ea.cpp
	#beosea.cpp
	#unios2.cpp

add_library(unrar SHARED ${UNRAR_SRC} ${UNRAR_PLATFORM_SRC})
add_definitions(-DRARDLL)
