# Maintainer: Jesse McClure AKA "Tribly" <jmcclure [at] cns [dot] umass [dot] edu>
pkgname=slider-git
_gitname="slider"
pkgver=1.69.b14a772
pkgrel=1
pkgdesc="PDF presentation tool"
arch=('any')
url="https://github.com/TrilbyWhite/Slider"
license=('GPL3')
depends=('poppler-glib' 'libxrandr')
makedepends=('git')
source=("$_gitname::git://github.com/TrilbyWhite/Slider.git")
sha256sums=('SKIP')

pkgver() {
	cd "${srcdir}/$_gitname"
	echo 1.$(git rev-list --count HEAD).$(git describe --always )
}

build() {
	cd "${srcdir}/$_gitname"
	make
}

package() {
	cd "${srcdir}/$_gitname"
	make DESTDIR="${pkgdir}" PREFIX=/usr install
}
