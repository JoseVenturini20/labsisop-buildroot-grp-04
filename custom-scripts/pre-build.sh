#!/bin/sh

cp $BASE_DIR/../custom-scripts/S41network-config $BASE_DIR/target/etc/init.d
cp $BASE_DIR/../custom-scripts/S50hello $BASE_DIR/target/etc/init.d
cp $BASE_DIR/../custom-scripts/TrabalhoPratico1/S51_TP1.sh $BASE_DIR/target/etc/init.d
cp $BASE_DIR/../custom-scripts/TrabalhoPratico1/httpServer $BASE_DIR/target/usr/bin

chmod +x $BASE_DIR/target/etc/init.d/S41network-config
chmod +x $BASE_DIR/target/etc/init.d/S50hello
chmod +x $BASE_DIR/target/etc/init.d/S51_TP1.sh
chmod +x $BASE_DIR/target/usr/bin/httpServer