
SRC=cpm.cpp dd.cpp dmk.cpp jv1.cpp jv3.cpp md.cpp nd.cpp \
	osi.cpp rd.cpp td1.cpp td3.cpp td4.cpp vdi.cpp v80.cpp


all:	v80

v80:	$(SRC)
	g++ ${CFLAGS} -o v80 $(SRC)

install:	v80
	install -s v80 /usr/local/bin/v80

clean:
	rm -f v80 


#
