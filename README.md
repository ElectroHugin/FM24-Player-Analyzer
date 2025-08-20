# FM Player Analysis Dashboard

A comprehensive Streamlit-based web application designed to import, analyze, and visualize player data from Football Manager. This tool helps you make informed decisions about player roles, tactical suitability, and squad depth by calculating a unique "Dynamic Weighted Role Score" (DWRS) for every player.

## Key Features

*   **HTML Data Import**: Easily upload your squad's HTML export from Football Manager to populate the dashboard.
*   **Dynamic Role Scoring (DWRS)**: Calculates a normalized percentage score for each player in every possible role, based on a weighted average of their attributes.
*   **Customizable Weighting System**: Fine-tune the attribute weights and role multipliers via a simple `config.ini` file to match your tactical preferences.
*   **Role Assignment & Management**: View all players, filter for those without assigned roles, and automatically or manually assign suitable roles based on their positions.
*   **In-Depth Role Analysis**: Select a specific role (e.g., Ball-Playing Defender) and see a ranked list of the best players for that role from your club, your second team, and your scouted list.
*   **Player-Role Matrix**: A powerful grid view showing the DWRS for all your players across every assigned role. Filter by tactic to see only the most relevant roles.
*   **Best Position Calculator**: Select a tactic and automatically generate your best Starting XI and B-Team based on the highest DWRS for each position.
*   **Player Comparison**: Select multiple players for a side-by-side comparison of their complete attribute profiles.
*   **DWRS Progress Tracking**: Monitor player development over time with line charts showing their historical DWRS in specific roles.

## How the Dynamic Weighted Role Score (DWRS) is Calculated

The DWRS is the core of this application, designed to provide a single, understandable score that evaluates a player's suitability for a specific role. The formula, which was developed specifically for this project, is built on a two-layer philosophy:

1.  A player's general effectiveness in the match engine ("meta" attributes).
2.  Their specific aptitude for the duties of a particular role (role-specific attributes).

Here is a step-by-step breakdown of how the score is calculated:

### Step 1: The Foundation - General "Meta" Attributes

The calculation starts with the idea that not all attributes are created equal in the Football Manager match engine. Based on extensive community testing, particularly the findings in **[this Reddit post by u/florin133](https://www.reddit.com/r/footballmanagergames/comments/16fuksi/a_not_so_short_guide_to_meta_player_attributes/)**, attributes are grouped into categories of importance:

*   **Extremely Important:** Pace, Acceleration
*   **Important:** Jumping Reach, Anticipation, Balance, Agility, Concentration, Finishing
*   **Good:** Work Rate, Dribbling, Stamina, Strength, Passing, Determination, Vision
*   **Decent:** Long Shots, Marking, Decisions, First Touch
*   **Almost Irrelevant:** Off the Ball, Tackling, Teamwork, Composure, Technique, Positioning

Each of these categories is assigned a base weight, establishing a foundational score for a player's general quality.

### Step 2: Specialization - Role-Specific Multipliers

A good general player isn't always the right player for a specific job. To reflect this, the DWRS applies a powerful multiplier to the attributes that Football Manager itself designates as "Key" or "Preferable" for a given role.

*   When calculating the score for a **Ball-Playing Defender**, attributes like Passing, Composure, and Vision receive a significant boost.
*   When calculating for a **Winger**, Crossing and Dribbling get the same boost.

This ensures that the final score heavily rewards players who are a perfect fit for the tactical instructions of a role.

### Step 3: The Calculation in Plain English

The final "Absolute" DWRS is calculated as follows:

1.  **Attribute Grouping:** All of a player's attributes are sorted into their "meta" categories.
2.  **Role-Specific Weighting:** Inside each category, attributes are weighted. A standard attribute has a weight of 1.0. However, if an attribute is "Preferable" for the role being analyzed, its value is multiplied by the `preferable_multiplier` (e.g., 4.0). If it's a "Key" attribute, it's multiplied by the even higher `key_multiplier` (e.g., 8.0).
3.  **Category Averages:** A weighted average is calculated for each "meta" category.
4.  **Final Sum:** The average of each category is then multiplied by that category's main weight (e.g., the "Extremely Important" average is multiplied by its weight) and all the results are added together to get the final score.

### Step 4: Normalization - Creating the Percentage Score

The "Absolute" score is just a number. To make it intuitive, it's converted to a percentage.

This is done by comparing the player's score to two theoretical benchmarks:
*   The **"Worst Possible Player":** A player with a value of 1 in every single attribute.
*   The **"Best Possible Player":** A player with a value of 20 in every single attribute.

The system calculates the absolute DWRS for both of these theoretical players. Your player's score is then placed on this scale to generate the final percentage. A score of **100%** would mean your player has a 20 in every attribute relevant to that role, weighted according to the system.

### Full Customization

The entire system is transparent and customizable. All base weights for the "meta" categories and the multipliers for "Key" and "Preferable" attributes can be easily changed in the `config/config.ini` file or directly on the **Settings** page of the application.

### Credits and Inspiration

This project stands on the shoulders of giants in the FM community.
*   The concept for weighting "meta" attributes was inspired by the research of **u/florin133**.
*   The initial idea and data view structures were heavily inspired by the excellent **FM Client App** ([fm-client-app.vercel.app](https://fm-client-app.vercel.app/)).
*   The custom views used for exporting player data from the game were created by the talented **PlayingSquirrel** ([@playingsquirrel on X/Twitter](https://x.com/playingsquirrel)). You can find these files [here](https://www.mediafire.com/file/ymf6xhw0bk4enjj/FM24_files.zip/file).

## Project Structure

The project is organized to separate different aspects of the application's logic, making it easier to maintain and extend.

```
your-project-folder/
├── config/
│   ├── config.ini          # User-configurable weights and multipliers.
│   └── definitions.json    # Core definitions for roles, tactics, and mappings.
├── databases/
│   └── *.db                # SQLite database files (ignored by git).
├── src/
│   ├── app.py              # Main Streamlit application, handles UI and pages.
│   ├── analytics.py        # Core logic for the DWRS calculation.
│   ├── data_parser.py      # Handles HTML parsing and data processing.
│   ├── sqlite_db.py        # Manages all database interactions.
│   ├── config_handler.py   # Helper functions to read/write config.ini.
│   ├── constants.py        # Loads and centralizes definitions from JSON.
│   └── ...                 # Other source files.
└── README.md               # This file.
```

## Installation

To run this application locally, follow these steps:

1.  **Clone the repository (when you upload it to GitHub):**
    ```bash
    git clone https://github.com/your-username/your-repo-name.git
    cd your-repo-name
    ```

2.  **Create and activate a virtual environment:**
    ```bash
    # For Windows
    python -m venv venv
    .\venv\Scripts\activate

    # For macOS/Linux
    python3 -m venv venv
    source venv/bin/activate
    ```

3.  **Install the required Python packages:**
    *Create a `requirements.txt` file with the following content:*
    ```
    streamlit
    pandas
    beautifulsoup4
    ```
    *Then run the installation command:*
    ```bash
    pip install -r requirements.txt
    ```

## How to Use

1.  **Run the Streamlit Application:**
    Navigate to the `src/` directory and run the following command in your terminal:
    ```bash
    streamlit run app.py
    ```
    Your web browser should automatically open with the application running.

2.  **Initial Setup:**
    *   The application will automatically create a `config/` and `databases/` directory in the project root if they don't exist.
    *   Use the sidebar to upload your first player HTML file exported from Football Manager. The data will be processed and saved automatically.
    *   Go to the "Settings" page or select "Your Club" in the sidebar to configure the dashboard for your team.

3.  **Explore the Pages:**
    *   **Assign Roles**: Start here to automatically assign roles to your players based on their natural positions.
    *   **Role Analysis**: Dive deep into specific roles to find the best-suited players.
    *   **Best Position Calculator**: See your strongest lineup for any of the pre-defined tactics.

## Configuration

You can customize the logic of the DWRS calculation without touching the code:

*   **`config/config.ini`**:
    *   `[Weights]`: Change the importance of global attribute categories (e.g., make "Pace" more or less critical).
    *   `[GKWeights]`: Adjust weights specifically for goalkeepers.
    *   `[RoleMultipliers]`: Increase or decrease the bonus given to "key" and "preferable" attributes for a specific role.

*   **`config/definitions.json`**:
    *   `"role_specific_weights"`: Define which attributes are "key" and "preferable" for each role.
    *   `"tactic_roles"`: Define your own custom tactics by mapping positions to specific roles.
    *   `"tactic_layouts"`: Define the visual layout for your custom tactics on the "Best Position Calculator" page.