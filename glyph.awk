
BEGIN {
  mode = 1;
}

function output_glyph(_, tail, offset, len, i, j, x, y, l) {
  tail = head;
  offset = 0;
  while (match(tail, /^[^[:space:]]+/) > 0) {
    len = RLENGTH;

    printf("{'%s', %d, {", substr(tail, 1, 1), len);
    for (i = 0; i < 7; i++) {
      x = substr(data[i], offset + 1, len);
      l = length(x);
      y = 0;
      for (j = 0; j < l; j++) {
        if (substr(x, j + 1, 1) ~ /[^[:space:]]/) {
          y = or(y, lshift(1, j))
        }
      }
      printf("%d, ", y);
    }
    printf("}},\n");

    match(tail, /^[^[:space:]]+[[:space:]]*/);
    offset += RLENGTH;
    tail = substr(tail, RLENGTH + 1);
  }
}

mode == 1 {
  if (/^[[:space:]]*(#|$)/) next;

  mode = 2;
  head = $0;
  iline = 0;
  next;
}

mode == 2 {
  if (/^[[:space:]]*$/) next;

  data[iline++] = $0
  if (iline == 7) {
    output_glyph();
    mode = 1;
  }
  next;
}
