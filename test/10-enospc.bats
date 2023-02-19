#!/usr/bin/env bats

teardown() {
  rm -f full
}

@test "disk full: percent specified" {
  run env LD_PRELOAD=libenospace.so LIBENOSPACE_OPT=percent LIBENOSPACE_AVAIL=-1 sh -c "echo test > full"
cat << EOF
$output
EOF
  [ "$status" -eq 1 ]
}

@test "disk full: bytes specified" {
  run env LD_PRELOAD=libenospace.so LIBENOSPACE_OPT=bytes LIBENOSPACE_AVAIL=-1 sh -c "echo test > full"
cat << EOF
$output
EOF
  [ "$status" -eq 1 ]
}

@test "disk limits disabled: percent specified" {
  run env LD_PRELOAD=libenospace.so LIBENOSPACE_OPT=percent LIBENOSPACE_AVAIL=0 sh -c "echo test > full"
cat << EOF
$output
EOF
  [ "$status" -eq 0 ]
}

@test "disk limits disabled: bytes specified" {
  run env LD_PRELOAD=libenospace.so LIBENOSPACE_OPT=bytes LIBENOSPACE_AVAIL=0 sh -c "echo test > full"
cat << EOF
$output
EOF
  [ "$status" -eq 0 ]
}

@test "disk almost full: bytes specified" {
  run env LD_PRELOAD=libenospace.so LIBENOSPACE_OPT=bytes LIBENOSPACE_AVAIL=5 sh -c "echo test > full"
cat << EOF
$output
$(cat full)
EOF
  [ "$status" -eq 0 ]
}
