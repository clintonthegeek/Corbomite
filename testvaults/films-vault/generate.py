#!/usr/bin/env python3
"""Generate the Films test vault for exercising Corbomite Bases (Cluster D.3).

Emits ~50 film notes (rich frontmatter: year/runtime/rating/genre/tags/
director-link/country/watched/decade) + director notes (so wikilink/file()
resolution from D.1 is exercised) + a multi-view Films.base and a Directors.base.

Reproducible: rerun to regenerate. Data is hand-curated, roughly accurate.
"""
import os
import re
import textwrap

HERE = os.path.dirname(os.path.abspath(__file__))

# (title, year, runtime_min, rating, primary_genre, [tags], director, country, watched)
FILMS = [
    ("2001: A Space Odyssey", 1968, 149, 8.3, "Sci-Fi", ["sci-fi", "space", "classic"], "Stanley Kubrick", "UK", True),
    ("The Godfather", 1972, 175, 9.2, "Crime", ["crime", "drama", "classic"], "Francis Ford Coppola", "USA", True),
    ("Chinatown", 1974, 130, 8.1, "Noir", ["noir", "mystery", "crime"], "Roman Polanski", "USA", True),
    ("Jaws", 1975, 124, 8.1, "Thriller", ["thriller", "horror"], "Steven Spielberg", "USA", True),
    ("Taxi Driver", 1976, 114, 8.2, "Drama", ["drama", "crime", "noir"], "Martin Scorsese", "USA", True),
    ("Star Wars", 1977, 121, 8.6, "Sci-Fi", ["sci-fi", "space", "adventure"], "George Lucas", "USA", True),
    ("Alien", 1979, 117, 8.5, "Sci-Fi", ["sci-fi", "horror", "space"], "Ridley Scott", "UK", True),
    ("Apocalypse Now", 1979, 147, 8.4, "War", ["war", "drama"], "Francis Ford Coppola", "USA", False),
    ("The Shining", 1980, 146, 8.4, "Horror", ["horror", "thriller"], "Stanley Kubrick", "UK", True),
    ("Blade Runner", 1982, 117, 8.1, "Sci-Fi", ["sci-fi", "noir", "dystopia"], "Ridley Scott", "USA", True),
    ("The Thing", 1982, 109, 8.2, "Horror", ["horror", "sci-fi"], "John Carpenter", "USA", True),
    ("Aliens", 1986, 137, 8.4, "Sci-Fi", ["sci-fi", "action", "space"], "James Cameron", "USA", True),
    ("Die Hard", 1988, 132, 8.2, "Action", ["action", "thriller"], "John McTiernan", "USA", True),
    ("Goodfellas", 1990, 145, 8.7, "Crime", ["crime", "drama"], "Martin Scorsese", "USA", True),
    ("Terminator 2: Judgment Day", 1991, 137, 8.6, "Sci-Fi", ["sci-fi", "action"], "James Cameron", "USA", True),
    ("The Silence of the Lambs", 1991, 118, 8.6, "Thriller", ["thriller", "horror", "crime"], "Jonathan Demme", "USA", True),
    ("Unforgiven", 1992, 130, 8.2, "Western", ["western", "drama"], "Clint Eastwood", "USA", False),
    ("Jurassic Park", 1993, 127, 8.2, "Sci-Fi", ["sci-fi", "adventure"], "Steven Spielberg", "USA", True),
    ("Pulp Fiction", 1994, 154, 8.9, "Crime", ["crime", "drama"], "Quentin Tarantino", "USA", True),
    ("The Shawshank Redemption", 1994, 142, 9.3, "Drama", ["drama"], "Frank Darabont", "USA", True),
    ("Heat", 1995, 170, 8.3, "Crime", ["crime", "thriller", "action"], "Michael Mann", "USA", True),
    ("Se7en", 1995, 127, 8.6, "Thriller", ["thriller", "crime", "noir"], "David Fincher", "USA", True),
    ("Fargo", 1996, 98, 8.1, "Crime", ["crime", "noir", "comedy"], "Joel Coen", "USA", True),
    ("The Big Lebowski", 1998, 117, 8.1, "Comedy", ["comedy", "crime"], "Joel Coen", "USA", True),
    ("The Matrix", 1999, 136, 8.7, "Sci-Fi", ["sci-fi", "action", "dystopia"], "Lana Wachowski", "USA", True),
    ("Fight Club", 1999, 139, 8.8, "Drama", ["drama", "thriller"], "David Fincher", "USA", True),
    ("In the Mood for Love", 2000, 98, 8.1, "Romance", ["romance", "drama"], "Wong Kar-wai", "Hong Kong", False),
    ("Spirited Away", 2001, 125, 8.6, "Animation", ["animation", "fantasy"], "Hayao Miyazaki", "Japan", True),
    ("The Lord of the Rings: The Fellowship of the Ring", 2001, 178, 8.9, "Fantasy", ["fantasy", "adventure"], "Peter Jackson", "New Zealand", True),
    ("City of God", 2002, 130, 8.6, "Crime", ["crime", "drama"], "Fernando Meirelles", "Brazil", False),
    ("Oldboy", 2003, 120, 8.3, "Thriller", ["thriller", "mystery"], "Park Chan-wook", "South Korea", False),
    ("No Country for Old Men", 2007, 122, 8.2, "Thriller", ["thriller", "crime", "western"], "Joel Coen", "USA", True),
    ("There Will Be Blood", 2007, 158, 8.2, "Drama", ["drama"], "Paul Thomas Anderson", "USA", False),
    ("The Dark Knight", 2008, 152, 9.0, "Action", ["action", "crime", "thriller"], "Christopher Nolan", "USA", True),
    ("Inception", 2010, 148, 8.8, "Sci-Fi", ["sci-fi", "action", "thriller"], "Christopher Nolan", "USA", True),
    ("Drive", 2011, 100, 7.8, "Noir", ["noir", "thriller", "crime"], "Nicolas Winding Refn", "USA", True),
    ("The Master", 2012, 138, 7.1, "Drama", ["drama"], "Paul Thomas Anderson", "USA", False),
    ("Her", 2013, 126, 8.0, "Sci-Fi", ["sci-fi", "romance", "drama"], "Spike Jonze", "USA", True),
    ("Mad Max: Fury Road", 2015, 120, 8.1, "Action", ["action", "sci-fi", "dystopia"], "George Miller", "Australia", True),
    ("Arrival", 2016, 116, 7.9, "Sci-Fi", ["sci-fi", "drama"], "Denis Villeneuve", "USA", True),
    ("Moonlight", 2016, 111, 7.4, "Drama", ["drama"], "Barry Jenkins", "USA", False),
    ("Blade Runner 2049", 2017, 164, 8.0, "Sci-Fi", ["sci-fi", "noir", "dystopia"], "Denis Villeneuve", "USA", True),
    ("Get Out", 2017, 104, 7.7, "Horror", ["horror", "thriller"], "Jordan Peele", "USA", True),
    ("Parasite", 2019, 132, 8.5, "Thriller", ["thriller", "drama", "comedy"], "Bong Joon-ho", "South Korea", True),
    ("Portrait of a Lady on Fire", 2019, 122, 8.1, "Romance", ["romance", "drama"], "Celine Sciamma", "France", False),
    ("Dune", 2021, 155, 8.0, "Sci-Fi", ["sci-fi", "adventure"], "Denis Villeneuve", "USA", True),
    ("Everything Everywhere All at Once", 2022, 139, 7.8, "Sci-Fi", ["sci-fi", "comedy", "action"], "Daniel Kwan", "USA", True),
    ("The Banshees of Inisherin", 2022, 114, 7.7, "Drama", ["drama", "comedy"], "Martin McDonagh", "Ireland", False),
    ("Oppenheimer", 2023, 180, 8.3, "Drama", ["drama", "war"], "Christopher Nolan", "USA", True),
    ("Past Lives", 2023, 105, 7.8, "Romance", ["romance", "drama"], "Celine Song", "USA", False),
]

# director -> (born_year, country)
DIRECTORS = {
    "Stanley Kubrick": (1928, "USA"),
    "Francis Ford Coppola": (1939, "USA"),
    "Roman Polanski": (1933, "France"),
    "Steven Spielberg": (1946, "USA"),
    "Martin Scorsese": (1942, "USA"),
    "George Lucas": (1944, "USA"),
    "Ridley Scott": (1937, "UK"),
    "John Carpenter": (1948, "USA"),
    "James Cameron": (1954, "Canada"),
    "John McTiernan": (1951, "USA"),
    "Jonathan Demme": (1944, "USA"),
    "Clint Eastwood": (1930, "USA"),
    "Quentin Tarantino": (1963, "USA"),
    "Frank Darabont": (1959, "USA"),
    "Michael Mann": (1943, "USA"),
    "David Fincher": (1962, "USA"),
    "Joel Coen": (1954, "USA"),
    "Lana Wachowski": (1965, "USA"),
    "Wong Kar-wai": (1958, "Hong Kong"),
    "Hayao Miyazaki": (1941, "Japan"),
    "Peter Jackson": (1961, "New Zealand"),
    "Fernando Meirelles": (1955, "Brazil"),
    "Park Chan-wook": (1963, "South Korea"),
    "Paul Thomas Anderson": (1970, "USA"),
    "Christopher Nolan": (1970, "UK"),
    "Nicolas Winding Refn": (1970, "Denmark"),
    "Spike Jonze": (1969, "USA"),
    "George Miller": (1945, "Australia"),
    "Denis Villeneuve": (1967, "Canada"),
    "Barry Jenkins": (1979, "USA"),
    "Jordan Peele": (1979, "USA"),
    "Bong Joon-ho": (1969, "South Korea"),
    "Celine Sciamma": (1978, "France"),
    "Daniel Kwan": (1988, "USA"),
    "Martin McDonagh": (1970, "Ireland"),
    "Celine Song": (1988, "Canada"),
}


def slug(name):
    """Filesystem-safe note basename (keep it readable; Obsidian allows spaces)."""
    return re.sub(r'[\\/:*?"<>|]', "", name).strip()


def yaml_list(items):
    return "[" + ", ".join(items) + "]"


def write(path, text):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8") as f:
        f.write(text)


def main():
    films_dir = os.path.join(HERE, "Films")
    dirs_dir = os.path.join(HERE, "Directors")

    # Film notes
    for (title, year, runtime, rating, genre, tags, director, country, watched) in FILMS:
        decade = (year // 10) * 10
        fm = textwrap.dedent(f"""\
            ---
            title: "{title}"
            year: {year}
            decade: {decade}
            runtime: {runtime}
            rating: {rating}
            genre: {genre}
            tags: {yaml_list(tags)}
            director: "[[{director}]]"
            country: {country}
            watched: {str(watched).lower()}
            ---

            # {title}

            *{genre}* directed by [[{director}]] ({country}, {year}).
            Runtime {runtime} min · rating {rating}.
            """)
        write(os.path.join(films_dir, f"{slug(title)}.md"), fm)

    # Director notes
    for name, (born, country) in DIRECTORS.items():
        fm = textwrap.dedent(f"""\
            ---
            type: director
            born: {born}
            country: {country}
            tags: [director]
            ---

            # {name}

            Film director ({country}, b. {born}).
            """)
        write(os.path.join(dirs_dir, f"{slug(name)}.md"), fm)

    # ---- Films.base : multi-view, exercises every D.3 surface ----
    films_base = textwrap.dedent("""\
        filters:
          and:
            - 'file.inFolder("Films")'
        formulas:
          hours: "(note.runtime / 60).round(2)"
        views:
          - type: table
            name: All Films
            order:
              - note.title
              - note.year
              - note.genre
              - note.rating
              - note.runtime
              - formula.hours
              - note.director
              - note.country
              - note.watched
            sort:
              - property: note.year
                direction: ASC
          - type: table
            name: By Genre
            groupBy:
              property: note.genre
              direction: ASC
            order:
              - note.title
              - note.year
              - note.rating
              - note.director
            sort:
              - property: note.genre
                direction: ASC
              - property: note.rating
                direction: DESC
            summaries:
              note.title: count
              note.rating: average
          - type: table
            name: By Decade
            groupBy:
              property: note.decade
              direction: ASC
            order:
              - note.title
              - note.year
              - note.genre
              - note.rating
            sort:
              - property: note.decade
                direction: ASC
              - property: note.rating
                direction: DESC
            summaries:
              note.title: count
              note.rating: average
          - type: table
            name: Watchlist
            filters:
              and:
                - "note.watched == false"
            order:
              - note.title
              - note.year
              - note.genre
              - note.rating
              - note.director
            sort:
              - property: note.rating
                direction: DESC
          - type: table
            name: Top Rated
            filters:
              and:
                - "note.rating >= 8"
            order:
              - note.title
              - note.year
              - note.genre
              - note.rating
            sort:
              - property: note.rating
                direction: DESC
            limit: 15
        """)
    write(os.path.join(HERE, "Films.base"), films_base)

    # ---- Directors.base : group films-by-director angle via the director notes ----
    directors_base = textwrap.dedent("""\
        filters:
          and:
            - 'note.type == "director"'
        views:
          - type: table
            name: Directors
            order:
              - file.name
              - note.born
              - note.country
            sort:
              - property: note.born
                direction: ASC
          - type: table
            name: By Country
            groupBy:
              property: note.country
              direction: ASC
            order:
              - file.name
              - note.born
            sort:
              - property: note.country
                direction: ASC
              - property: note.born
                direction: ASC
            summaries:
              file.name: count
        """)
    write(os.path.join(HERE, "Directors.base"), directors_base)

    # Minimal .obsidian so it's unambiguously a vault root.
    write(os.path.join(HERE, ".obsidian", "app.json"), "{}\n")

    n_films = len(FILMS)
    n_dirs = len(DIRECTORS)
    print(f"Wrote {n_films} films, {n_dirs} directors, Films.base (5 views), Directors.base (2 views).")


if __name__ == "__main__":
    main()
