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

The DWRS is the core of this application, designed to provide a single, understandable score that evaluates a player's suitability for a specific role. The formula is built on a two-layer philosophy:

1.  A player's general effectiveness based on attributes that are powerful in the match engine ("meta" attributes).
2.  Their specific aptitude for the duties of a particular role (role-specific attributes).

Here is a step-by-step breakdown:

#### Step 1: Foundational Attribute Weighting

The calculation starts with the idea that not all attributes are created equal. Based on extensive community research, particularly the findings in **[this Reddit post by u/florin133](https://www.reddit.com/r/footballmanagergames/comments/16fuksi/a_not_so_short_guide_to_meta_player_attributes/)**, attributes are grouped into "meta" categories of importance:

*   **Extremely Important:** Pace, Acceleration (Default Weight: 8.0)
*   **Important:** Jumping Reach, Anticipation, Balance, Agility, Concentration, Finishing (Default Weight: 4.0)
*   **Good:** Work Rate, Dribbling, Stamina, Strength, Passing, Determination, Vision (Default Weight: 2.0)
*   **Decent:** Long Shots, Marking, Decisions, First Touch (Default Weight: 1.0)
*   **Almost Irrelevant:** Off the Ball, Tackling, Teamwork, Composure, Technique, Positioning (Default Weight: 0.2)

Each of these categories is assigned a base weight, establishing a foundational score for a player's general quality.

#### Step 2: Role-Specific Multipliers

A good general player isn't always the right player for a specific job. To reflect this, the DWRS applies a multiplier directly to an attribute's value (1-20) if it is designated as "Key" or "Preferable" for a given role.

*   When calculating the score for a **Ball-Playing Defender**, attributes like Passing, Composure, and Vision get their values boosted.
*   When calculating for a **Winger**, Crossing and Dribbling get the same boost.

The default multipliers are:
*   **`key_multiplier`**: **1.5x**
*   **`preferable_multiplier`**: **1.2x**

#### Step 3: The Calculation

The final "Absolute" DWRS is calculated as follows:

1.  **Attribute Boosting:** For the role being analyzed (e.g., Winger), each of a player's attributes is checked. If an attribute like Dribbling (with a value of 16) is "Key" for a Winger, its value is boosted for this calculation: `16 * 1.5 = 24`. If another attribute is "Preferable", it is multiplied by 1.2. All other attributes keep their original value.
2.  **Category Averages:** A simple average of these (potentially boosted) attribute values is calculated for each "meta" category from Step 1.
3.  **Final Score:** The average of each category is then multiplied by that category's main weight (e.g., the "Extremely Important" average is multiplied by its 8.0 weight). All results are summed to get the final "Absolute" DWRS.

#### Step 4: Normalization (The Percentage Score)

To make the score intuitive, the "Absolute" score is converted to a percentage. This is done by comparing the player's score to two theoretical benchmarks: the score of a player with 1 in every attribute (the "Worst") and a player with 20 in every attribute (the "Best"). Your player's score is then scaled between these two points to generate the final percentage. A score of **100%** means your player has, effectively, a 20 in every attribute relevant to that role, weighted according to the system.

#### Full Customization

The entire system is transparent. All base weights for the "meta" categories and the multipliers for "Key" and "Preferable" attributes can be easily changed on the **Settings** page of the application or in the `config/config.ini` file.

### Credits and Inspiration

This project stands on the shoulders of giants in the FM community.
*   The concept for weighting "meta" attributes was inspired by the research of **u/florin133**.
*   The initial idea and data view structures were heavily inspired by the excellent **FM Client App** ([fm-client-app.vercel.app](https://fm-client-app.vercel.app/)).
*   The custom views used for exporting player data from the game were created by the talented **PlayingSquirrel** ([@playingsquirrel on X/Twitter](https://x.com/playingsquirrel)). You can find these files [here](https://www.mediafire.com/file/ymf6xhw0bk4enjj/FM24_files.zip/file).

## About the Included Tactical Formations

The tactical presets available in the "Player-Role Matrix" and "Best Position Calculator" pages serve as a powerful, standardized baseline for player analysis. These are not random formations; they have been selected from the **FM-Arena Best FM24 Tactics Hall of Fame**, a community-driven effort to test and rank the most effective tactics available.

You can view the full list, see detailed performance metrics, and download the original `.fmf` tactic files from the official source:

*   **[FM-Arena FM24 Hall of Fame](https://fm-arena.com/table/fm24-hall-of-fame/)**

Full credit for the creation, innovation, and rigorous testing of these tactics goes to their respective authors and the entire FM-Arena team. The roles and layouts have been replicated within this application to allow users to quickly evaluate their squads against some of the game's most proven and meta-defining tactical systems.

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