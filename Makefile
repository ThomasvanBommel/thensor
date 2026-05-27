.PHONY: build sh

build:
	docker run --rm -ti -v .:/app espressif/esp-matter:release-v1.4.2_idf_v5.4.1 sh -c "cd /app && idf.py build"

sh:
	docker run --rm -ti -v .:/app espressif/esp-matter:release-v1.4.2_idf_v5.4.1 sh -c "cd /app && bash"
