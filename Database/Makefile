PSQL = psql --host=zack.fritz.box --username=plaque_admin --dbname=plaque

all: Debug Hash Bonjour Auth-Profiles Auth-Devices Auth-Anticipations Auth-TANs Journal Reception

Journal:
	$(PSQL) < Journal.sql
