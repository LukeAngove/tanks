#! /usr/bin/awk -f

function esc(s) {
    gsub(/&/, "\\&amp;", s);
    gsub(/</, "\\&lt;", s);
    gsub(/>/, "\\&gt;", s);
    return s;
}

BEGIN {
    ngames = 20;

    print "<!DOCTYPE html>";
    print "<html>";
    print "  <head>";
    print "    <title>Dirtbags Tanks</title>";
    print "    <link rel=\"stylesheet\" href=\"style.css\" type=\"text/css\">";
    print "  </head>";
    print "  <body>";
    print "    <h1>Dirtbags Tanks</h1>";

    print "    <p>New here?  Read the <a href=\"intro.html\">introduction</a>.</p>";
    print "    <p>New round every minute.</p>";
    print "    <h2>Rankings</h2>";
    print "    <p>Over the last 20 games only.</p>";
    print "    <ol>";
    for (i = 1; i < ARGC; i += 1) {
        id = ARGV[i];

        if (1 == getline < (id "/name")) {
            names[id] = esc($0);
        } else {
            names[id] = "<i>Unnamed</i>";
        }

        getline < (id "/color");
        if (/^#[0-9A-Fa-f]+$/) {
            color[id] = $0;
        } else {
            color[id] = "#c0c0c0";
        }


        for (j = 0; 1 == getline < (id "/points"); j += 1) {
            pts[id, j % ngames] = int($0);
        }
        total = 0;
        for (j = 0; j < ngames; j += 1) {
            total += pts[id, j];
        }
        scores[total] = total;
        points[id] = total;
    }
    while (1) {
        # Find highest score
        maxscore = -1;
        for (p in scores) {
            if (int(p) > maxscore) {
                maxscore = int(p);
            }
        }
        if (maxscore == -1) {
            break;
        }
        delete scores[maxscore];

        for (id in points) {
            if (points[id] == maxscore) {
                printf("<li><span class=\"swatch\" style=\"background-color: %s;\">#</span> %s (%d points)</li>\n", color[id], names[id], points[id]);
            }
        }
    }
    print "    </ol>";

    print "    <h2>Rounds</h2>";
    print "    <ul>";
    getline rounds < "next-round";
    for (i = rounds - 1; i >= 0; i -= 1) {
        printf("<li><a href=\"round-%04d.html\">%04d</a></li>\n", i, i);
    }
    print "    </ul>";

    while (getline < ENVIRON["NAV_HTML_INC"]) {
        print;
    }

    print "  </body>";
    print "</html>";
}
