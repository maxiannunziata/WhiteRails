# Maintainer: Tu Nombre <tu@email.com>
pkgname=wr-runtime
pkgver=0.1
pkgrel=0
pkgdesc="WhiteRails native semantic runtime"
url="https://github.com/whiterails/runtime" # Cambiar si tenés otro repo
arch="x86_64" # Cambiar por aarch64 si corresponde
license="MIT"
depends="openrc"
makedepends="gcc make musl-dev"
options="!check !strip"
provider_priority=50
source="$pkgname-$pkgver.tar.gz"

build() {
	cd "$srcdir/$pkgname-$pkgver"
	make CC="$CC" CFLAGS="$CFLAGS"
}

package() {
	install -Dm755 "$srcdir/$pkgname-$pkgver/wr_runtime" "$pkgdir/usr/bin/wr_runtime"

	# Init script
	install -Dm755 "$srcdir/$pkgname-$pkgver/init.d/whiterails" "$pkgdir/etc/init.d/whiterails"

	# Servicios JSON
	install -d "$pkgdir/var/lib/whiterails/services"
	cp "$srcdir/$pkgname-$pkgver/services/"*.json "$pkgdir/var/lib/whiterails/services/"
	chmod 0644 "$pkgdir/var/lib/whiterails/services/"*.json
	chown root:root "$pkgdir/var/lib/whiterails/services/"*.json

	# Acciones (placeholder en caso de que se exporten más adelante)
	install -d "$pkgdir/usr/share/whiterails/actions"
}
